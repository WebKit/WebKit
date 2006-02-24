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
#import "MouseEvent.h"

namespace WebCore {

static MouseButton mouseButtonForEvent(NSEvent *event)
{
    switch ([event type]) {
        case NSLeftMouseDown:
        case NSLeftMouseUp:
        case NSLeftMouseDragged:
        default:
            return LeftButton;
        case NSRightMouseDown:
        case NSRightMouseUp:
        case NSRightMouseDragged:
            return RightButton;
        case NSOtherMouseDown:
        case NSOtherMouseUp:
        case NSOtherMouseDragged:
            return MiddleButton;
    }
}

static IntPoint positionForEvent(NSEvent *event)
{
    switch ([event type]) {
        case NSLeftMouseDown:
        case NSLeftMouseUp:
        case NSLeftMouseDragged:
        case NSRightMouseDown:
        case NSRightMouseUp:
        case NSRightMouseDragged:
        case NSOtherMouseDown:
        case NSOtherMouseUp:
        case NSOtherMouseDragged:
        case NSMouseMoved:
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
        case NSLeftMouseDown:
        case NSLeftMouseUp:
        case NSLeftMouseDragged:
        case NSRightMouseDown:
        case NSRightMouseUp:
        case NSRightMouseDragged:
        case NSOtherMouseDown:
        case NSOtherMouseUp:
        case NSOtherMouseDragged:
        case NSMouseMoved:
        case NSScrollWheel: {
            NSPoint point = [[event window] convertBaseToScreen:[event locationInWindow]];
            point.y = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - point.y;
            return IntPoint(point);
        }
        default:
            return IntPoint();
    }
}

static int clickCountForEvent(NSEvent *event)
{
    switch ([event type]) {
        case NSLeftMouseDown:
        case NSLeftMouseUp:
        case NSLeftMouseDragged:
        case NSRightMouseDown:
        case NSRightMouseUp:
        case NSRightMouseDragged:
        case NSOtherMouseDown:
        case NSOtherMouseUp:
        case NSOtherMouseDragged:
            return [event clickCount];
        default:
            return 0;
    }
}

MouseEvent::MouseEvent(NSEvent* event)
    : m_position(positionForEvent(event))
    , m_globalPosition(globalPositionForEvent(event))
    , m_button(mouseButtonForEvent(event))
    , m_clickCount(clickCountForEvent(event))
    , m_shiftKey([event modifierFlags] & NSShiftKeyMask)
    , m_ctrlKey([event modifierFlags] & NSControlKeyMask)
    , m_altKey([event modifierFlags] & NSAlternateKeyMask)
    , m_metaKey([event modifierFlags] & NSCommandKeyMask)
{
}

MouseEvent::MouseEvent()
    : m_button(LeftButton), m_clickCount(0), m_shiftKey(false), m_ctrlKey(false), m_altKey(false), m_metaKey(false)
{
    NSEvent* event = [NSApp currentEvent];
    if (event) {
        m_position = positionForEvent(event);
        m_globalPosition = globalPositionForEvent(event);
        m_button = mouseButtonForEvent(event);
        m_clickCount = clickCountForEvent(event);
        m_shiftKey = [event modifierFlags] & NSShiftKeyMask;
        m_ctrlKey = [event modifierFlags] & NSControlKeyMask;
        m_altKey = [event modifierFlags] & NSAlternateKeyMask;
        m_metaKey = [event modifierFlags] & NSCommandKeyMask;
    }
}

}
