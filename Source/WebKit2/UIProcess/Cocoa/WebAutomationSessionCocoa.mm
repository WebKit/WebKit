/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "WebAutomationSession.h"

#import "WebPageProxy.h"
#import "_WKAutomationSession.h"
#import <WebCore/IntPoint.h>
#import <WebCore/IntSize.h>
#import <WebCore/PlatformMouseEvent.h>
#import <objc/runtime.h>

using namespace WebCore;

namespace WebKit {

#if USE(APPKIT)

void WebAutomationSession::sendSynthesizedEventsToPage(WebPageProxy& page, NSArray *eventsToSend)
{
    NSWindow *window = page.platformWindow();

    for (NSEvent *event in eventsToSend) {
        // FIXME: <https://www.webkit.org/b/155963> Set an associated object with a per-session identifier.
        // This will allow a WebKit2 client to tell apart events synthesized on behalf of an automation command.
        [window sendEvent:event];
    }
}

void WebAutomationSession::platformSimulateMouseInteraction(WebPageProxy& page, const WebCore::IntPoint& viewPosition, Inspector::Protocol::Automation::MouseInteraction interaction, Inspector::Protocol::Automation::MouseButton button, WebEvent::Modifiers keyModifiers)
{
    IntRect windowRect;
    page.rootViewToWindow(IntRect(viewPosition, IntSize()), windowRect);
    IntPoint windowPosition = windowRect.location();

    NSEventModifierFlags modifiers = 0;
    if (keyModifiers & WebEvent::MetaKey)
        modifiers |= NSEventModifierFlagCommand;
    if (keyModifiers & WebEvent::AltKey)
        modifiers |= NSEventModifierFlagOption;
    if (keyModifiers & WebEvent::ControlKey)
        modifiers |= NSEventModifierFlagControl;
    if (keyModifiers & WebEvent::ShiftKey)
        modifiers |= NSEventModifierFlagShift;

    NSTimeInterval timestamp = [NSDate timeIntervalSinceReferenceDate];
    NSWindow *window = page.platformWindow();
    NSInteger windowNumber = window.windowNumber;

    NSEventType downEventType;
    NSEventType dragEventType;
    NSEventType upEventType;
    switch (button) {
    case Inspector::Protocol::Automation::MouseButton::None:
        downEventType = upEventType = dragEventType = NSEventTypeMouseMoved;
        break;
    case Inspector::Protocol::Automation::MouseButton::Left:
        downEventType = NSEventTypeLeftMouseDown;
        dragEventType = NSEventTypeLeftMouseDragged;
        upEventType = NSEventTypeLeftMouseUp;
        break;
    case Inspector::Protocol::Automation::MouseButton::Middle:
        downEventType = NSEventTypeOtherMouseDown;
        dragEventType = NSEventTypeLeftMouseDragged;
        upEventType = NSEventTypeOtherMouseUp;
        break;
    case Inspector::Protocol::Automation::MouseButton::Right:
        downEventType = NSEventTypeRightMouseDown;
        upEventType = NSEventTypeRightMouseUp;
        break;
    }

    NSMutableArray<NSEvent *> *eventsToBeSent = [NSMutableArray new];

    switch (interaction) {
    case Inspector::Protocol::Automation::MouseInteraction::Move:
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:dragEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:0 clickCount:0 pressure:0.0f]];
        break;
    case Inspector::Protocol::Automation::MouseInteraction::Down:
        // Hard-code the click count to one, since clients don't expect successive simulated
        // down/up events to be potentially counted as a double click event.
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:downEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:0 clickCount:1 pressure:WebCore::ForceAtClick]];
        break;
    case Inspector::Protocol::Automation::MouseInteraction::Up:
        // Hard-code the click count to one, since clients don't expect successive simulated
        // down/up events to be potentially counted as a double click event.
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:upEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:0 clickCount:1 pressure:0.0f]];
        break;
    case Inspector::Protocol::Automation::MouseInteraction::SingleClick:
        // Send separate down and up events. WebCore will see this as a single-click event.
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:downEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:0 clickCount:1 pressure:WebCore::ForceAtClick]];
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:upEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:0 clickCount:1 pressure:0.0f]];
        break;
    case Inspector::Protocol::Automation::MouseInteraction::DoubleClick:
        // Send multiple down and up events with proper click count.
        // WebCore will see this as a single-click event then double-click event.
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:downEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:0 clickCount:1 pressure:WebCore::ForceAtClick]];
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:upEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:0 clickCount:1 pressure:0.0f]];
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:downEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:0 clickCount:2 pressure:WebCore::ForceAtClick]];
        [eventsToBeSent addObject:[NSEvent mouseEventWithType:upEventType location:windowPosition modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:0 clickCount:2 pressure:0.0f]];
    }

    sendSynthesizedEventsToPage(page, eventsToBeSent);
}

#endif // USE(APPKIT)

} // namespace WebKit
