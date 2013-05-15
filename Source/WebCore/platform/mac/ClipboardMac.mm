/*
 * Copyright (C) 2004, 2005, 2006, 2008, 2010, 2013 Apple Inc. All rights reserved.
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
#import "ClipboardMac.h"

#import "CachedImage.h"
#import "DOMElementInternal.h"
#import "Document.h"
#import "DragClient.h"
#import "DragController.h"
#import "DragData.h"
#import "Frame.h"
#import "FrameSnapshottingMac.h"
#import "Page.h"
#import "Pasteboard.h"
#import "PasteboardStrategy.h"
#import "PlatformStrategies.h"

namespace WebCore {

#if ENABLE(DRAG_SUPPORT)
PassRefPtr<Clipboard> Clipboard::create(ClipboardAccessPolicy policy, DragData* dragData, Frame* frame)
{
    return ClipboardMac::create(DragAndDrop, dragData->pasteboardName(), policy, dragData->containsFiles() ? ClipboardMac::DragAndDropFiles : ClipboardMac::DragAndDropData, frame);
}
#endif

PassRefPtr<Clipboard> Clipboard::createForDragAndDrop()
{
    return ClipboardMac::create(Clipboard::DragAndDrop, NSDragPboard, ClipboardWritable, ClipboardMac::DragAndDropData, 0);
}

PassRefPtr<Clipboard> Clipboard::createForCopyAndPaste(ClipboardAccessPolicy policy)
{
    return ClipboardMac::create(Clipboard::CopyAndPaste,
        policy == ClipboardWritable ? platformStrategies()->pasteboardStrategy()->uniqueName() : String(NSGeneralPboard), policy, ClipboardMac::CopyAndPasteGeneric, 0);
}

ClipboardMac::ClipboardMac(ClipboardType clipboardType, const String& pasteboardName, ClipboardAccessPolicy policy, ClipboardContents clipboardContents)
    : Clipboard(policy, clipboardType, Pasteboard::create(pasteboardName), clipboardContents == DragAndDropFiles)
{
}

#if ENABLE(DRAG_SUPPORT)
void Clipboard::declareAndWriteDragImage(Element* element, const KURL& url, const String& title, Frame* frame)
{
    ASSERT(frame);
    if (Page* page = frame->page())
        page->dragController()->client()->declareAndWriteDragImage(m_pasteboard->name(), kit(element), url, title, frame);
}
#endif
    
DragImageRef Clipboard::createDragImage(IntPoint& location) const
{
    NSImage *result = nil;
    if (m_dragImageElement) {
        Document* document = m_dragImageElement->document();
        if (Frame* frame = document->frame()) {
            NSRect imageRect;
            NSRect elementRect;
            result = snapshotDragImage(frame, m_dragImageElement.get(), &imageRect, &elementRect);
            // Client specifies point relative to element, not the whole image, which may include child
            // layers spread out all over the place.
            location.setX(elementRect.origin.x - imageRect.origin.x + m_dragLoc.x());
            location.setY(imageRect.size.height - (elementRect.origin.y - imageRect.origin.y + m_dragLoc.y()));
        }
    } else if (m_dragImage) {
        result = m_dragImage->image()->getNSImage();
        
        location = m_dragLoc;
        location.setY([result size].height - location.y());
    }
    return result;
}

}
