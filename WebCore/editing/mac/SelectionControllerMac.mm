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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
 
#import "config.h"
#import "SelectionController.h"

#import "AXObjectCache.h"
#import "Frame.h"
#import "RenderView.h"
#import "WebCoreViewFactory.h"

namespace WebCore {

void SelectionController::notifyAccessibilityForSelectionChange()
{
    Document* document = m_frame->document();

    if (AXObjectCache::accessibilityEnabled() && m_sel.start().isNotNull() && m_sel.end().isNotNull())
        document->axObjectCache()->postNotification(m_sel.start().node()->renderer(), "AXSelectedTextChanged", false);

    // if zoom feature is enabled, insertion point changes should update the zoom
    if (!UAZoomEnabled() || !m_sel.isCaret())
        return;

    RenderView* renderView = document->renderView();
    if (!renderView)
        return;
    FrameView* frameView = m_frame->view();
    if (!frameView)
        return;

    IntRect selectionRect = absoluteCaretBounds();
    IntRect viewRect = renderView->viewRect();

    selectionRect = frameView->contentsToScreen(selectionRect);
    viewRect = frameView->contentsToScreen(viewRect);
    CGRect cgCaretRect = CGRectMake(selectionRect.x(), selectionRect.y(), selectionRect.width(), selectionRect.height());
    CGRect cgViewRect = CGRectMake(viewRect.x(), viewRect.y(), viewRect.width(), viewRect.height());
    cgCaretRect = [[WebCoreViewFactory sharedFactory] accessibilityConvertScreenRect:cgCaretRect];
    cgViewRect = [[WebCoreViewFactory sharedFactory] accessibilityConvertScreenRect:cgViewRect];

    UAZoomChangeFocus(&cgViewRect, &cgCaretRect, kUAZoomFocusTypeInsertionPoint);
}

} // namespace WebCore
