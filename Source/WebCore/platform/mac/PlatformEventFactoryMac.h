/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PlatformWheelEvent.h>
#include <wtf/Platform.h>

#if PLATFORM(MAC)

#if defined(__OBJC__)
#include <AppKit/AppKit.h>
#endif

namespace WebCore {

class PlatformEventFactory {
public:
    WEBCORE_EXPORT static PlatformMouseEvent createPlatformMouseEvent(NSEvent *, NSEvent *correspondingPressureEvent, NSView *windowView);
    static PlatformWheelEvent createPlatformWheelEvent(NSEvent *, NSView *windowView);
    WEBCORE_EXPORT static PlatformKeyboardEvent createPlatformKeyboardEvent(NSEvent *);
};

#if PLATFORM(COCOA) && defined(__OBJC__)

// FIXME: This function doesn't really belong in this header.
WEBCORE_EXPORT NSPoint globalPoint(const NSPoint& windowPoint, NSWindow *);

WEBCORE_EXPORT MouseButton mouseButtonForEvent(NSEvent *);
WEBCORE_EXPORT unsigned short currentlyPressedMouseButtons();
WEBCORE_EXPORT PlatformEvent::Type mouseEventTypeForEvent(NSEvent *);
WEBCORE_EXPORT int clickCountForEvent(NSEvent *);
WEBCORE_EXPORT NSPoint globalPointForEvent(NSEvent *);
WEBCORE_EXPORT DoublePoint pointForEvent(NSEvent *, NSView *windowView);

WEBCORE_EXPORT bool isKeyUpEvent(NSEvent *);
WEBCORE_EXPORT int windowsKeyCodeForKeyEvent(NSEvent *);
WEBCORE_EXPORT String keyIdentifierForKeyEvent(NSEvent *);
WEBCORE_EXPORT String keyForKeyEvent(NSEvent *);
WEBCORE_EXPORT String codeForKeyEvent(NSEvent *);
WEBCORE_EXPORT UInt8 keyCharForEvent(NSEvent *);
WEBCORE_EXPORT String textFromEvent(NSEvent *, bool replacesSoftSpace = false);
WEBCORE_EXPORT String unmodifiedTextFromEvent(NSEvent *, bool replacesSoftSpace = false);
WEBCORE_EXPORT bool isKeypadEvent(NSEvent *);

WEBCORE_EXPORT DoublePoint unadjustedMovementForEvent(NSEvent *);

WEBCORE_EXPORT OptionSet<PlatformEvent::Modifier> modifiersForEvent(NSEvent *);
WEBCORE_EXPORT OptionSet<PlatformEvent::Modifier> modifiersForModifierFlags(NSEventModifierFlags);

WEBCORE_EXPORT void getWheelEventDeltas(NSEvent *, float& deltaX, float& deltaY, BOOL& continuous);

#endif

} // namespace WebCore

#endif // PLATFORM(MAC)
