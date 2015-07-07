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
#import "DataTransfer.h"

#import "CachedImage.h"
#import "Element.h"
#import "DragImage.h"

namespace WebCore {

// FIXME: Need to refactor and figure out how to handle the flipping in a more sensible way so we can
// use the default DataTransfer::dragImage from DataTransfer.cpp. Note also that this handles cases that
// DataTransfer::dragImage in DataTransfer.cpp does not handle correctly, so must resolve that as well.
DragImageRef DataTransfer::createDragImage(IntPoint& location) const
{
    DragImageRef result = nil;
    if (m_dragImageElement) {
        if (Frame* frame = m_dragImageElement->document().frame()) {
            IntRect imageRect;
            IntRect elementRect;
            result = createDragImageForImage(*frame, *m_dragImageElement, imageRect, elementRect);
            // Client specifies point relative to element, not the whole image, which may include child
            // layers spread out all over the place.
            location.setX(elementRect.x() - imageRect.x() + m_dragLocation.x());
            location.setY(imageRect.height() - (elementRect.y() - imageRect.y() + m_dragLocation.y()));
        }
    } else if (m_dragImage) {
        result = m_dragImage->image()->getNSImage();
        
        location = m_dragLocation;
        location.setY([result size].height - location.y());
    }
    return result;
}

}
