/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "Viewport.h"

#include "DocumentPage.h"
#include "DocumentView.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Page.h"
#include "ViewportSegments.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Viewport);

Viewport::Viewport(LocalDOMWindow& window)
    : LocalDOMWindowProperty(&window)
{
}

const Vector<Ref<DOMRect>>& Viewport::segments() const
{
    if (m_segmentsCacheValid)
        return m_cachedSegments;

    m_cachedSegments.clear();
    m_segmentsCacheValid = true;

    RefPtr frame = this->frame();
    if (!frame)
        return m_cachedSegments;

    RefPtr page = frame->page();
    if (!page)
        return m_cachedSegments;

    auto& viewportSegments = page->viewportSegments();

    if (viewportSegments.columns > 1 || viewportSegments.rows > 1) {
        m_cachedSegments.reserveInitialCapacity(viewportSegments.segmentRects.size());
        for (auto& rect : viewportSegments.segmentRects)
            m_cachedSegments.append(DOMRect::create(rect));
    } else {
        RefPtr view = frame->view();
        if (view) {
            FloatRect viewportRect = view->visualViewportRect();
            m_cachedSegments.append(DOMRect::create(FloatRect(0, 0, viewportRect.width(), viewportRect.height())));
        }
    }

    return m_cachedSegments;
}

void Viewport::invalidateSegments()
{
    m_segmentsCacheValid = false;
    m_cachedSegments.clear();
}

} // namespace WebCore
