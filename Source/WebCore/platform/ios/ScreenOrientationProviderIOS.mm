/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)
#import "ScreenOrientationProvider.h"

#import <pal/ios/UIKitSoftLink.h>

@interface WebScreenOrientationObserver : NSObject
@property (nonatomic) WebCore::ScreenOrientationProvider* provider;
@end

@implementation WebScreenOrientationObserver {
}

- (WebScreenOrientationObserver *)initWithProvider:(WebCore::ScreenOrientationProvider&)provider
{
    self = [super init];
    if (!self)
        return nil;

    _provider = &provider;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_screenOrientationDidChange) name:PAL::get_UIKit_UIApplicationDidChangeStatusBarOrientationNotification() object:nil];
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self name:PAL::get_UIKit_UIApplicationDidChangeStatusBarOrientationNotification() object:nil];
    [super dealloc];
}

- (void)_screenOrientationDidChange
{
    // We need to make sure we notify the client on the main thread.
    callOnMainRunLoop([self, protectedSelf = RetainPtr<WebScreenOrientationObserver>(self)] {
        if (_provider)
            _provider->screenOrientationDidChange();
    });
}

@end

namespace WebCore {

std::optional<ScreenOrientationType> ScreenOrientationProvider::platformCurrentOrientation()
{
    auto window = m_window.get();
    if (!window)
        return std::nullopt;

    UIWindowScene *scene = window.get().windowScene;
    switch ([scene interfaceOrientation]) {
    case UIInterfaceOrientationUnknown:
    case UIInterfaceOrientationPortrait:
        break;
    case UIInterfaceOrientationPortraitUpsideDown:
        return WebCore::ScreenOrientationType::PortraitSecondary;
    case UIInterfaceOrientationLandscapeLeft:
        return WebCore::ScreenOrientationType::LandscapePrimary;
    case UIInterfaceOrientationLandscapeRight:
        return WebCore::ScreenOrientationType::LandscapeSecondary;
    }
    return WebCore::ScreenOrientationType::PortraitPrimary;
}

void ScreenOrientationProvider::platformStartListeningForChanges()
{
    ASSERT(!m_systemObserver);
    m_systemObserver = adoptNS([[WebScreenOrientationObserver alloc] initWithProvider:*this]);
}

void ScreenOrientationProvider::platformStopListeningForChanges()
{
    if (!m_systemObserver)
        return;

    m_systemObserver.get().provider = nullptr;
    m_systemObserver = nullptr;
}

void ScreenOrientationProvider::setWindow(UIWindow *window)
{
    m_window = window;
    if (window)
        screenOrientationDidChange();
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
