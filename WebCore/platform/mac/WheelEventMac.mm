/*
 * Copyright (C) 2004, 2006, 2010 Apple Computer, Inc.  All rights reserved.
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

#import "PlatformMouseEvent.h"
#import "Scrollbar.h"
#import "WebCoreSystemInterface.h"

namespace WebCore {

PlatformWheelEvent::PlatformWheelEvent(NSEvent* event, NSView *windowView)
    : m_position(pointForEvent(event, windowView))
    , m_globalPosition(globalPointForEvent(event))
    , m_granularity(ScrollByPixelWheelEvent)
    , m_isAccepted(false)
    , m_shiftKey([event modifierFlags] & NSShiftKeyMask)
    , m_ctrlKey([event modifierFlags] & NSControlKeyMask)
    , m_altKey([event modifierFlags] & NSAlternateKeyMask)
    , m_metaKey([event modifierFlags] & NSCommandKeyMask)
{
    BOOL continuous;
    wkGetWheelEventDeltas(event, &m_deltaX, &m_deltaY, &m_wheelTicksX, &m_wheelTicksY, &continuous);

    if (!continuous) {
        m_deltaX *= static_cast<float>(Scrollbar::pixelsPerLineStep());
        m_deltaY *= static_cast<float>(Scrollbar::pixelsPerLineStep());
    }
}

} // namespace WebCore
