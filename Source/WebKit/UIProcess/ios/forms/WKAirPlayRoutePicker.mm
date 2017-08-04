/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "WKAirPlayRoutePicker.h"

#if PLATFORM(IOS)

#import "UIKitSPI.h"
#import "WKContentView.h"
#import <WebCore/MediaPlayerSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
SOFT_LINK_FRAMEWORK(MediaPlayer)
SOFT_LINK_CLASS(MediaPlayer, MPAVRoutingController)
SOFT_LINK_CLASS(MediaPlayer, MPMediaControlsViewController)
#endif

using namespace WebKit;

@implementation WKAirPlayRoutePicker {
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    RetainPtr<MPMediaControlsViewController> _actionSheet;
#endif
}

- (void)dealloc
{
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    [_actionSheet dismissViewControllerAnimated:0 completion:nil];
#endif
    [super dealloc];
}

- (void)showFromView:(WKContentView *)view
{
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    if (_actionSheet)
        return;

    __block RetainPtr<MPAVRoutingController> routingController = adoptNS([allocMPAVRoutingControllerInstance() initWithName:@"WebKit - HTML media element showing AirPlay route picker"]);
    [routingController setDiscoveryMode:MPRouteDiscoveryModeDetailed];

    _actionSheet = adoptNS([allocMPMediaControlsViewControllerInstance() init]);
    _actionSheet.get().didDismissHandler = ^ {
        [routingController setDiscoveryMode:MPRouteDiscoveryModeDisabled];
        routingController = nil;
        _actionSheet = nil;
    };

    UIViewController *viewControllerForPresentation = [UIViewController _viewControllerForFullScreenPresentationFromView:view];
    [viewControllerForPresentation presentViewController:_actionSheet.get() animated:YES completion:nil];
#else
    UNUSED_PARAM(view);
#endif
}

@end

#endif // PLATFORM(IOS)
