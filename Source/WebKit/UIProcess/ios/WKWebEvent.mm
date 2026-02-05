/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKWebEvent.h"

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import <wtf/RetainPtr.h>

@implementation WKWebEvent {
    RetainPtr<UIEvent> _uiEvent;
}

- (instancetype)initWithEvent:(UIEvent *)event
{
    uint16_t keyCode;
    UIKeyboardInputFlags inputFlags;
    NSInteger modifierFlags;
    static auto physicalKeyboardEventClass = NSClassFromString(@"UIPhysicalKeyboardEvent");
    BOOL isHardwareKeyboardEvent = [event isKindOfClass:physicalKeyboardEventClass] && event._hidEvent;
    RetainPtr<UIEvent> uiEvent;
    if (!isHardwareKeyboardEvent) {
        keyCode = 0;
        inputFlags = (UIKeyboardInputFlags)0;
        modifierFlags = 0;
        uiEvent = event;
    } else {
        // FIXME: `checked_objc_cast<UIPhysicalKeyboardEvent>(event)` causes a linking error.
        SUPPRESS_MEMORY_UNSAFE_CAST UIPhysicalKeyboardEvent *keyEvent = (UIPhysicalKeyboardEvent *)event;
        keyCode = keyEvent._keyCode;
        inputFlags = keyEvent._inputFlags;
        modifierFlags = keyEvent._gsModifierFlags;
        uiEvent = adoptNS([keyEvent _cloneEvent]); // UIKit uses a singleton for hardware keyboard events.
    }

    self = [super initWithKeyEventType:([uiEvent _isKeyDown] ? WebEventKeyDown : WebEventKeyUp) timeStamp:[uiEvent timestamp] characters:[uiEvent _modifiedInput] charactersIgnoringModifiers:[uiEvent _unmodifiedInput] modifiers:modifierFlags isRepeating:(inputFlags & kUIKeyboardInputRepeat) withFlags:inputFlags withInputManagerHint:nil keyCode:keyCode isTabKey:[[uiEvent _modifiedInput] isEqualToString:@"\t"]];
    if (!self)
        return nil;

    _uiEvent = WTF::move(uiEvent);

    return self;
}

- (UIEvent *)uiEvent
{
    return _uiEvent.get();
}

@end

#endif // PLATFORM(IOS_FAMILY)
