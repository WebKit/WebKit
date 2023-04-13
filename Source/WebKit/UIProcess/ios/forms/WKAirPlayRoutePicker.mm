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

#if PLATFORM(IOS_FAMILY) && ENABLE(AIRPLAY_PICKER)

#import "UIKitSPI.h"
#import <WebCore/AudioSession.h>
#import <pal/spi/ios/MediaPlayerSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

SOFT_LINK_FRAMEWORK(MediaPlayer)
SOFT_LINK_CLASS(MediaPlayer, MPAVRoutingController)
SOFT_LINK_CLASS(MediaPlayer, MPMediaControlsConfiguration)
SOFT_LINK_CLASS(MediaPlayer, MPMediaControlsViewController)

@interface MPMediaControlsConfiguration (WKMPMediaControlsConfiguration)
@property (nonatomic) BOOL sortByIsVideoRoute;
@end

enum {
    WKAirPlayRoutePickerRouteSharingPolicyDefault = 0,
    WKAirPlayRoutePickerRouteSharingPolicyLongFormAudio = 1,
    WKAirPlayRoutePickerRouteSharingPolicyIndependent = 2,
    WKAirPlayRoutePickerRouteSharingPolicyLongFormVideo = 3,
};
typedef NSInteger WKAirPlayRoutePickerRouteSharingPolicy;

@interface MPMediaControlsViewController (WKMPMediaControlsViewControllerPrivate)
- (instancetype)initWithConfiguration:(MPMediaControlsConfiguration *)configuration;
- (void)setOverrideRouteSharingPolicy:(WKAirPlayRoutePickerRouteSharingPolicy)routeSharingPolicy routingContextUID:(NSString *)routingContextUID;
@end

@implementation WKAirPlayRoutePicker {
    RetainPtr<MPMediaControlsViewController> _actionSheet;
}

- (void)dealloc
{
    [_actionSheet dismissViewControllerAnimated:0 completion:nil];
    [super dealloc];
}

- (void)showFromView:(UIView *)view routeSharingPolicy:(WebCore::RouteSharingPolicy)routeSharingPolicy routingContextUID:(NSString *)routingContextUID hasVideo:(BOOL)hasVideo
{
    static_assert(static_cast<size_t>(WebCore::RouteSharingPolicy::Default) == static_cast<size_t>(WKAirPlayRoutePickerRouteSharingPolicyDefault), "RouteSharingPolicy::Default is not WKAirPlayRoutePickerRouteSharingPolicyDefault as expected");
    static_assert(static_cast<size_t>(WebCore::RouteSharingPolicy::LongFormAudio) == static_cast<size_t>(WKAirPlayRoutePickerRouteSharingPolicyLongFormAudio), "RouteSharingPolicy::LongFormAudio is not WKAirPlayRoutePickerRouteSharingPolicyLongFormAudio as expected");
    static_assert(static_cast<size_t>(WebCore::RouteSharingPolicy::Independent) == static_cast<size_t>(WKAirPlayRoutePickerRouteSharingPolicyIndependent), "RouteSharingPolicy::Independent is not WKAirPlayRoutePickerRouteSharingPolicyIndependent as expected");
    static_assert(static_cast<size_t>(WebCore::RouteSharingPolicy::LongFormVideo) == static_cast<size_t>(WKAirPlayRoutePickerRouteSharingPolicyLongFormVideo), "RouteSharingPolicy::LongFormVideo is not WKAirPlayRoutePickerRouteSharingPolicyLongFormVideo as expected");
    if (_actionSheet)
        return;

    __block RetainPtr<MPAVRoutingController> routingController = adoptNS([allocMPAVRoutingControllerInstance() initWithName:@"WebKit - HTML media element showing AirPlay route picker"]);
    [routingController setDiscoveryMode:MPRouteDiscoveryModeDetailed];

    RetainPtr<MPMediaControlsConfiguration> configuration;
    if ([getMPMediaControlsConfigurationClass() instancesRespondToSelector:@selector(setSortByIsVideoRoute:)]) {
        configuration = adoptNS([allocMPMediaControlsConfigurationInstance() init]);
        configuration.get().sortByIsVideoRoute = hasVideo;
    }
    _actionSheet = adoptNS([allocMPMediaControlsViewControllerInstance() initWithConfiguration:configuration.get()]);

    if ([_actionSheet respondsToSelector:@selector(setOverrideRouteSharingPolicy:routingContextUID:)])
        [_actionSheet setOverrideRouteSharingPolicy:static_cast<WKAirPlayRoutePickerRouteSharingPolicy>(routeSharingPolicy) routingContextUID:routingContextUID];

    _actionSheet.get().didDismissHandler = ^ {
        [routingController setDiscoveryMode:MPRouteDiscoveryModeDisabled];
        routingController = nil;
        _actionSheet = nil;
    };

    UIViewController *viewControllerForPresentation = [UIViewController _viewControllerForFullScreenPresentationFromView:view];
    [viewControllerForPresentation presentViewController:_actionSheet.get() animated:YES completion:nil];
}

@end

ALLOW_DEPRECATED_DECLARATIONS_END

#endif // PLATFORM(IOS_FAMILY) && ENABLE(AIRPLAY_PICKER)
