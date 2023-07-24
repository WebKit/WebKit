/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#import "PlatformWebView.h"

#import "OffscreenWindow.h"
#import <Carbon/Carbon.h>
#import <WebKit/WKPagePrivateMac.h>
#import <WebKit/WKRetainPtr.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

void PlatformWebView::initialize(WKPageConfigurationRef pageConfiguration)
{
    NSRect rect = NSMakeRect(0, 0, 800, 600);

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    if (auto* context = WKPageConfigurationGetContext(pageConfiguration))
        configuration.get().processPool = (WKProcessPool *)context;
    if (auto* controller = WKPageConfigurationGetUserContentController(pageConfiguration))
        configuration.get().userContentController = (WKUserContentController *)controller;
    if (auto* dataStore = WKPageConfigurationGetWebsiteDataStore(pageConfiguration))
        configuration.get().websiteDataStore = (WKWebsiteDataStore *)dataStore;
    if (auto* relatedPage = WKPageConfigurationGetRelatedPage(pageConfiguration))
        configuration.get()._relatedWebView = WKPageGetWebView(relatedPage);
    if (auto* preferences = WKPageConfigurationGetPreferences(pageConfiguration))
        configuration.get().preferences = (WKPreferences *)preferences;

    m_view = [[WKWebView alloc] initWithFrame:rect configuration:configuration.get()];
    [m_view _setWindowOcclusionDetectionEnabled:NO];

    m_window = [[OffscreenWindow alloc] initWithSize:NSSizeToCGSize(rect.size)];
    [[m_window contentView] addSubview:m_view];
}

PlatformWebView::PlatformWebView(WKPageConfigurationRef configuration)
{
    initialize(configuration);
}

PlatformWebView::PlatformWebView(WKContextRef contextRef)
{
    WKRetainPtr<WKPageConfigurationRef> configuration = adoptWK(WKPageConfigurationCreate());
    
    WKPageConfigurationSetContext(configuration.get(), contextRef);
    
    initialize(configuration.get());
}

PlatformWebView::PlatformWebView(WKPageRef relatedPage)
{
    WKRetainPtr<WKPageConfigurationRef> configuration = adoptWK(WKPageConfigurationCreate());
    
    WKPageConfigurationSetContext(configuration.get(), WKPageGetContext(relatedPage));
    WKPageConfigurationSetRelatedPage(configuration.get(), relatedPage);

    auto relatedConfiguration = adoptWK(WKPageCopyPageConfiguration(relatedPage));
    if (auto* preferences = WKPageConfigurationGetPreferences(relatedConfiguration.get()))
        WKPageConfigurationSetPreferences(configuration.get(), preferences);

    initialize(configuration.get());
}

PlatformWebView::~PlatformWebView()
{
    [m_window close];
    [m_window release];
    [m_view release];
}

void PlatformWebView::resizeTo(unsigned width, unsigned height)
{
    [m_view setFrame:NSMakeRect(0, 0, width, height)];
}

WKPageRef PlatformWebView::page() const
{
    return [m_view _pageRefForTransitionToWKWebView];
}

void PlatformWebView::focus()
{
    // Implement.
}

void PlatformWebView::simulateSpacebarKeyPress()
{
    NSEvent *event = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                      location:NSMakePoint(5, 5)
                                 modifierFlags:0
                                     timestamp:GetCurrentEventTime()
                                  windowNumber:[m_window windowNumber]
                                       context:[NSGraphicsContext currentContext]
                                    characters:@" "
                   charactersIgnoringModifiers:@" "
                                     isARepeat:NO
                                       keyCode:0x31];

    [m_view keyDown:event];

    event = [NSEvent keyEventWithType:NSEventTypeKeyUp
                             location:NSMakePoint(5, 5)
                        modifierFlags:0
                            timestamp:GetCurrentEventTime()
                         windowNumber:[m_window windowNumber]
                              context:[NSGraphicsContext currentContext]
                           characters:@" "
          charactersIgnoringModifiers:@" "
                            isARepeat:NO
                              keyCode:0x31];

    [m_view keyUp:event];
}

void PlatformWebView::simulateRightClick(unsigned x, unsigned y)
{
    NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
                                        location:NSMakePoint(x, y)
                                   modifierFlags:0
                                       timestamp:GetCurrentEventTime()
                                    windowNumber:[m_window windowNumber]
                                         context:[NSGraphicsContext currentContext]
                                     eventNumber:0
                                      clickCount:0
                                        pressure:0];


    [m_view rightMouseDown:event];

    event = [NSEvent mouseEventWithType:NSEventTypeRightMouseUp
                               location:NSMakePoint(x, y)
                          modifierFlags:0
                              timestamp:GetCurrentEventTime()
                           windowNumber:[m_window windowNumber]
                                context:[NSGraphicsContext currentContext]
                            eventNumber:0
                             clickCount:0
                               pressure:0];

    [m_view rightMouseUp:event];

}

static NSEventType eventTypeForButton(WKEventMouseButton button)
{
    switch (button) {
    case kWKEventMouseButtonLeftButton:
        return NSEventTypeLeftMouseDown;
    case kWKEventMouseButtonRightButton:
        return NSEventTypeRightMouseDown;
    case kWKEventMouseButtonMiddleButton:
        return NSEventTypeOtherMouseDown;
    case kWKEventMouseButtonNoButton:
        return NSEventTypeLeftMouseDown;
    }

    return NSEventTypeLeftMouseDown;
}

static NSEventModifierFlags modifierFlagsForWKModifiers(WKEventModifiers modifiers)
{
    NSEventModifierFlags returnVal = 0;
    if (modifiers & kWKEventModifiersShiftKey)
        returnVal |= NSEventModifierFlagShift;
    if (modifiers & kWKEventModifiersControlKey)
        returnVal |= NSEventModifierFlagControl;
    if (modifiers & kWKEventModifiersAltKey)
        returnVal |= NSEventModifierFlagOption;
    if (modifiers & kWKEventModifiersMetaKey)
        returnVal |= NSEventModifierFlagCommand;
    if (modifiers & kWKEventModifiersCapsLockKey)
        returnVal |= NSEventModifierFlagCapsLock;

    return returnVal;
}

void PlatformWebView::simulateMouseMove(unsigned x, unsigned y, WKEventModifiers modifiers)
{
    NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeMouseMoved location:NSMakePoint(x, y) modifierFlags:modifierFlagsForWKModifiers(modifiers) timestamp:GetCurrentEventTime() windowNumber:[m_window windowNumber] context:[NSGraphicsContext currentContext] eventNumber:0 clickCount:0 pressure:0];
    [m_view _simulateMouseMove:event];
}

void PlatformWebView::simulateButtonClick(WKEventMouseButton button, unsigned x, unsigned y, WKEventModifiers modifiers)
{
    NSEvent *event = [NSEvent mouseEventWithType:eventTypeForButton(button)
                                        location:NSMakePoint(x, y)
                                   modifierFlags:modifierFlagsForWKModifiers(modifiers)
                                       timestamp:GetCurrentEventTime()
                                    windowNumber:[m_window windowNumber]
                                         context:[NSGraphicsContext currentContext]
                                     eventNumber:0
                                      clickCount:0
                                        pressure:0];

    [m_view mouseDown:event];
}

} // namespace TestWebKitAPI
