/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 
#import "config.h"
#import "FrameSelection.h"

#import "AXObjectCache.h"
#import "Frame.h"
#import "RenderView.h"

namespace WebCore {

void FrameSelection::notifyAccessibilityForSelectionChange(const AXTextStateChangeIntent& intent)
{
    Document* document = m_frame->document();

    if (m_selection.start().isNotNull() && m_selection.end().isNotNull()) {
        if (AXObjectCache* cache = document->existingAXObjectCache())
            cache->postTextStateChangeNotification(m_selection.start(), intent, m_selection);
    }

#if !PLATFORM(IOS_FAMILY)
    // if zoom feature is enabled, insertion point changes should update the zoom
    if (!UAZoomEnabled() || !m_selection.isCaret())
        return;

    RenderView* renderView = document->renderView();
    if (!renderView)
        return;
    FrameView* frameView = m_frame->view();
    if (!frameView)
        return;

    IntRect selectionRect = absoluteCaretBounds();
    IntRect viewRect = snappedIntRect(renderView->viewRect());

    selectionRect = frameView->contentsToScreen(selectionRect);
    viewRect = frameView->contentsToScreen(viewRect);
    NSRect nsCaretRect = NSMakeRect(selectionRect.x(), selectionRect.y(), selectionRect.width(), selectionRect.height());
    NSRect nsViewRect = NSMakeRect(viewRect.x(), viewRect.y(), viewRect.width(), viewRect.height());
    nsCaretRect = toUserSpaceForPrimaryScreen(nsCaretRect);
    nsViewRect = toUserSpaceForPrimaryScreen(nsViewRect);
    CGRect cgCaretRect = NSRectToCGRect(nsCaretRect);
    CGRect cgViewRect = NSRectToCGRect(nsViewRect);

    UAZoomChangeFocus(&cgViewRect, &cgCaretRect, kUAZoomFocusTypeInsertionPoint);
#endif // !PLATFORM(IOS_FAMILY)
}

} // namespace WebCore
