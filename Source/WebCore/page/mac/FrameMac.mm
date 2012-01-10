/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#import "config.h"
#import "Frame.h"

#import "BlockExceptions.h"
#import "ColorMac.h"
#import "Cursor.h"
#import "DOMInternal.h"
#import "Event.h"
#import "FrameLoaderClient.h"
#import "FrameView.h"
#import "GraphicsContext.h"
#import "HTMLNames.h"
#import "HTMLTableCellElement.h"
#import "HitTestRequest.h"
#import "HitTestResult.h"
#import "KeyboardEvent.h"
#import "Logging.h"
#import "MouseEventWithHitTestResults.h"
#import "Page.h"
#import "PlatformKeyboardEvent.h"
#import "PlatformWheelEvent.h"
#import "RegularExpression.h"
#import "RenderTableCell.h"
#import "RenderView.h"
#import "Scrollbar.h"
#import "SimpleFontData.h"
#import "visible_units.h"
#import <wtf/StdLibExtras.h>

@interface NSView (WebCoreHTMLDocumentView)
- (void)drawSingleRect:(NSRect)rect;
@end
 
using namespace std;

namespace WebCore {

using namespace HTMLNames;

NSImage* Frame::imageFromRect(NSRect rect) const
{
    PaintBehavior oldBehavior = m_view->paintBehavior();
    m_view->setPaintBehavior(oldBehavior | PaintBehaviorFlattenCompositingLayers);
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    NSImage* resultImage = [[[NSImage alloc] initWithSize:rect.size] autorelease];
    
    if (rect.size.width != 0 && rect.size.height != 0) {
        [resultImage setFlipped:YES];
        [resultImage lockFocus];

        GraphicsContext graphicsContext((CGContextRef)[[NSGraphicsContext currentContext] graphicsPort]);        
        graphicsContext.save();
        graphicsContext.translate(-rect.origin.x, -rect.origin.y);
        m_view->paintContents(&graphicsContext, IntRect(rect));
        graphicsContext.restore();

        [resultImage unlockFocus];
        [resultImage setFlipped:NO];
    }
    
    m_view->setPaintBehavior(oldBehavior);
    return resultImage;
    
    END_BLOCK_OBJC_EXCEPTIONS;
    
    m_view->setPaintBehavior(oldBehavior);
    return nil;
}

NSImage* Frame::selectionImage(bool forceBlackText) const
{
    m_view->setPaintBehavior(PaintBehaviorSelectionOnly | (forceBlackText ? PaintBehaviorForceBlackText : 0));
    m_doc->updateLayout();
    NSImage* result = imageFromRect(selection()->bounds());
    m_view->setPaintBehavior(PaintBehaviorNormal);
    return result;
}

NSImage *Frame::rangeImage(Range* range, bool forceBlackText) const
{
    m_view->setPaintBehavior(PaintBehaviorSelectionOnly | (forceBlackText ? PaintBehaviorForceBlackText : 0));
    m_doc->updateLayout();
    RenderView* view = contentRenderer();
    if (!view)
        return nil;

    Position start = range->startPosition();
    Position candidate = start.downstream();
    if (candidate.deprecatedNode() && candidate.deprecatedNode()->renderer())
        start = candidate;

    Position end = range->endPosition();
    candidate = end.upstream();
    if (candidate.deprecatedNode() && candidate.deprecatedNode()->renderer())
        end = candidate;

    if (start.isNull() || end.isNull() || start == end)
        return nil;

    RenderObject* savedStartRenderer;
    int savedStartOffset;
    RenderObject* savedEndRenderer;
    int savedEndOffset;
    view->getSelection(savedStartRenderer, savedStartOffset, savedEndRenderer, savedEndOffset);

    RenderObject* startRenderer = start.deprecatedNode()->renderer();
    if (!startRenderer)
        return nil;

    RenderObject* endRenderer = end.deprecatedNode()->renderer();
    if (!endRenderer)
        return nil;

    view->setSelection(startRenderer, start.deprecatedEditingOffset(), endRenderer, end.deprecatedEditingOffset(), RenderView::RepaintNothing);
    NSImage* result = imageFromRect(view->selectionBounds());
    view->setSelection(savedStartRenderer, savedStartOffset, savedEndRenderer, savedEndOffset, RenderView::RepaintNothing);

    m_view->setPaintBehavior(PaintBehaviorNormal);
    return result;
}

NSImage* Frame::snapshotDragImage(Node* node, NSRect* imageRect, NSRect* elementRect) const
{
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return nil;
    
    renderer->updateDragState(true);    // mark dragged nodes (so they pick up the right CSS)
    m_doc->updateLayout();        // forces style recalc - needed since changing the drag state might
                                        // imply new styles, plus JS could have changed other things
    LayoutRect topLevelRect;
    NSRect paintingRect = renderer->paintingRootRect(topLevelRect);

    m_view->setNodeToDraw(node);              // invoke special sub-tree drawing mode
    NSImage* result = imageFromRect(paintingRect);
    renderer->updateDragState(false);
    m_doc->updateLayout();
    m_view->setNodeToDraw(0);

    if (elementRect)
        *elementRect = topLevelRect;
    if (imageRect)
        *imageRect = paintingRect;
    return result;
}

DragImageRef Frame::nodeImage(Node* node)
{
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return nil;

    m_doc->updateLayout(); // forces style recalc

    LayoutRect topLevelRect;
    NSRect paintingRect = renderer->paintingRootRect(topLevelRect);

    m_view->setNodeToDraw(node); // invoke special sub-tree drawing mode
    NSImage* result = imageFromRect(paintingRect);
    m_view->setNodeToDraw(0);

    return result;
}

DragImageRef Frame::dragImageForSelection()
{
    if (!selection()->isRange())
        return nil;
    return selectionImage();
}

} // namespace WebCore
