/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "PlatformWheelEvent.h"

namespace WebCore {

static IntPoint positionForEvent(NSEvent *event)
{
    switch ([event type]) {
        case NSScrollWheel:
            // Note: This has its origin at the bottom left of the window.
            // The Y coordinate gets flipped by ScrollView::viewportToContents.
            // We should probably change both this and that to not use "bottom left origin" coordinates at all.
            return IntPoint([event locationInWindow]);
        default:
            return IntPoint();
    }
}

static IntPoint globalPositionForEvent(NSEvent *event)
{
    switch ([event type]) {
        case NSScrollWheel: {
            NSPoint point = [[event window] convertBaseToScreen:[event locationInWindow]];
            point.y = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - point.y;
            return IntPoint(point);
        }
        default:
            return IntPoint();
    }
}

static bool eventIsHorizontal(NSEvent *event)
{
    switch ([event type]) {
        case NSScrollWheel:
            return [event deltaX] != 0;
        default:
            return false;
    }
}

static int deltaForEvent(NSEvent *event)
{
    switch ([event type]) {
        case NSScrollWheel:
            return lrint((eventIsHorizontal(event) ? [event deltaX] : [event deltaY]) * 120);
        default:
            return 0;
    }
}

PlatformWheelEvent::PlatformWheelEvent(NSEvent* event)
    : m_position(positionForEvent(event))
    , m_globalPosition(globalPositionForEvent(event))
    , m_delta(deltaForEvent(event))
    , m_isHorizontal(eventIsHorizontal(event))
    , m_isAccepted(false)
    , m_shiftKey([event modifierFlags] & NSShiftKeyMask)
    , m_ctrlKey([event modifierFlags] & NSControlKeyMask)
    , m_altKey([event modifierFlags] & NSAlternateKeyMask)
    , m_metaKey([event modifierFlags] & NSCommandKeyMask)
{
}

}
