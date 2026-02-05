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
#import "Chrome.h"
#import "ChromeClient.h"
#import "DocumentPage.h"
#import "DocumentView.h"
#import "LocalFrame.h"
#import "RenderView.h"

namespace WebCore {

void FrameSelection::notifyAccessibilityForSelectionChange(const AXTextStateChangeIntent& intent)
{
    if (!AXObjectCache::accessibilityEnabled())
        return;

    if (CheckedPtr cache = m_document->existingAXObjectCache()) {
        if (m_selection.start().isNotNull() && m_selection.end().isNotNull())
            cache->postTextStateChangeNotification(m_selection.start(), intent, m_selection);
        else {
            // The selection was cleared, so use `onSelectedTextChanged` with an empty selection range to update the isolated tree.
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=279524: In the future, we should consider actually doing
            // `postTextStateChangeNotification`, since right now, VoiceOver does not announce when text becomes unselected
            // (because we don't post a notification), unless new text becomes selected at the same time. However, even if we
            // did post this notification, changes would be needed in VoiceOver too, so just do onSelectedTextChanged for now.
            // Handling selection removals should also not be platform-specific (as it is here with ENABLE(ACCESSIBILITY_ISOLATED_TREE)).
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
            cache->onSelectedTextChanged(m_selection);
#endif
        }
    }

#if !PLATFORM(IOS_FAMILY)
    // if zoom feature is enabled, insertion point changes should update the zoom
    if (!m_selection.isCaret())
        return;

    auto* renderView = m_document->renderView();
    if (!renderView)
        return;
    RefPtr frameView = m_document->view();
    if (!frameView)
        return;

    IntRect selectionRect = absoluteCaretBounds();
    IntRect viewRect = snappedIntRect(renderView->viewRect());

    auto remoteFrameOffset = frameView->frame().loader().client().accessibilityRemoteFrameOffset();
    selectionRect.moveBy({ remoteFrameOffset });
    viewRect.moveBy({ remoteFrameOffset });

    selectionRect = frameView->contentsToScreen(selectionRect);
    viewRect = frameView->contentsToScreen(viewRect);

    if (!m_document->page())
        return;
    
    m_document->page()->chrome().client().changeUniversalAccessZoomFocus(viewRect, selectionRect);
#endif // !PLATFORM(IOS_FAMILY)
}

} // namespace WebCore
