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

#if PLATFORM(IOS) && ENABLE(AIRPLAY_PICKER)

#import "UIKitSPI.h"
#import <WebCore/AudioSession.h>
#import <pal/spi/ios/MediaPlayerSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

#if __IPHONE_OS_VERSION_MIN_REQUIRED < 110000 || PLATFORM(WATCHOS) || PLATFORM(APPLETV)
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WebPageProxy.h"

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

SOFT_LINK_FRAMEWORK(MediaPlayer)
SOFT_LINK_CLASS(MediaPlayer, MPAVRoutingController)
SOFT_LINK_CLASS(MediaPlayer, MPAudioVideoRoutingPopoverController)
SOFT_LINK_CLASS(MediaPlayer, MPAVRoutingSheet)

using namespace WebKit;

@implementation WKAirPlayRoutePicker {
    RetainPtr<MPAVRoutingController> _routingController;
    RetainPtr<MPAudioVideoRoutingPopoverController> _popoverController;  // iPad
    RetainPtr<MPAVRoutingSheet> _actionSheet; // iPhone
    WKContentView* _view; // Weak reference.
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;

    _view = view;
    return self;
}

- (void)dealloc
{
    // The ActionSheet's completion handler will release and clear the ActionSheet.
    [_actionSheet dismiss];
    [self _dismissAirPlayRoutePickerIPad];

    [super dealloc];
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)popoverControllerDidDismissPopover:(UIPopoverController *)popoverController
IGNORE_WARNINGS_END
{
    if (popoverController != _popoverController)
        return;

    [self _dismissAirPlayRoutePickerIPad];
}

- (void)_presentAirPlayPopoverAnimated:(BOOL)animated fromRect:(CGRect)elementRect
{
    [_popoverController presentPopoverFromRect:elementRect inView:_view permittedArrowDirections:UIPopoverArrowDirectionAny animated:animated];
}

- (void)_windowWillRotate:(NSNotification *)notification
{
    [_popoverController dismissPopoverAnimated:NO];
}

- (void)_windowDidRotate:(NSNotification *)notification
{
    [self _dismissAirPlayRoutePickerIPad];
}

- (void)_dismissAirPlayRoutePickerIPad
{
    if (!_routingController)
        return;

    [_routingController setDiscoveryMode:MPRouteDiscoveryModeDisabled];
    _routingController = nil;

    if (!_popoverController)
        return;

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center removeObserver:self name:UIWindowWillRotateNotification object:nil];
    [center removeObserver:self name:UIWindowDidRotateNotification object:nil];

    [_popoverController dismissPopoverAnimated:NO];
    [_popoverController setDelegate:nil];
    _popoverController = nil;
}

- (void)showAirPlayPickerIPad:(MPAVItemType)itemType fromRect:(CGRect)elementRect
{
    if (_popoverController)
        return;

    _popoverController = adoptNS([allocMPAudioVideoRoutingPopoverControllerInstance() initWithType:itemType]);
    [_popoverController setDelegate:self];

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(_windowWillRotate:) name:UIWindowWillRotateNotification object:nil];
    [center addObserver:self selector:@selector(_windowDidRotate:) name:UIWindowDidRotateNotification object:nil];

    [self _presentAirPlayPopoverAnimated:YES fromRect:elementRect];
}

- (void)showAirPlayPickerIPhone:(MPAVItemType)itemType
{
    if (_actionSheet)
        return;

    _actionSheet = adoptNS([allocMPAVRoutingSheetInstance() initWithAVItemType:itemType]);

    [_actionSheet showInView:_view withCompletionHandler:^{
        [_routingController setDiscoveryMode:MPRouteDiscoveryModeDisabled];
        _routingController = nil;
        _actionSheet = nil;
    }];
}

- (void)show:(BOOL)hasVideo fromRect:(CGRect)elementRect
{
    _routingController = adoptNS([allocMPAVRoutingControllerInstance() initWithName:@"WebKit2 - HTML media element showing AirPlay route picker"]);
    [_routingController setDiscoveryMode:MPRouteDiscoveryModeDetailed];

    MPAVItemType itemType = hasVideo ? MPAVItemTypeVideo : MPAVItemTypeAudio;
    if (currentUserInterfaceIdiomIsPad())
        [self showAirPlayPickerIPad:itemType fromRect:elementRect];
    else
        [self showAirPlayPickerIPhone:itemType];
}

@end

ALLOW_DEPRECATED_DECLARATIONS_END

#else 

SOFT_LINK_FRAMEWORK(MediaPlayer)
SOFT_LINK_CLASS(MediaPlayer, MPAVRoutingController)
SOFT_LINK_CLASS(MediaPlayer, MPMediaControlsViewController)

enum {
    WKAirPlayRoutePickerRouteSharingPolicyDefault = 0,
    WKAirPlayRoutePickerRouteSharingPolicyLongForm = 1,
    WKAirPlayRoutePickerRouteSharingPolicyIndependent = 2,
};
typedef NSInteger WKAirPlayRoutePickerRouteSharingPolicy;

@interface MPMediaControlsViewController (WKMPMediaControlsViewControllerPrivate)
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

- (void)showFromView:(UIView *)view routeSharingPolicy:(WebCore::RouteSharingPolicy)routeSharingPolicy routingContextUID:(NSString *)routingContextUID
{
    static_assert(static_cast<size_t>(WebCore::RouteSharingPolicy::Default) == static_cast<size_t>(WKAirPlayRoutePickerRouteSharingPolicyDefault), "RouteSharingPolicy::Default is not WKAirPlayRoutePickerRouteSharingPolicyDefault as expected");
    static_assert(static_cast<size_t>(WebCore::RouteSharingPolicy::LongForm) == static_cast<size_t>(WKAirPlayRoutePickerRouteSharingPolicyLongForm), "RouteSharingPolicy::LongForm is not WKAirPlayRoutePickerRouteSharingPolicyLongForm as expected");
    static_assert(static_cast<size_t>(WebCore::RouteSharingPolicy::Independent) == static_cast<size_t>(WKAirPlayRoutePickerRouteSharingPolicyIndependent), "RouteSharingPolicy::Independent is not WKAirPlayRoutePickerRouteSharingPolicyIndependent as expected");

    if (_actionSheet)
        return;

    __block RetainPtr<MPAVRoutingController> routingController = adoptNS([allocMPAVRoutingControllerInstance() initWithName:@"WebKit - HTML media element showing AirPlay route picker"]);
    [routingController setDiscoveryMode:MPRouteDiscoveryModeDetailed];

    _actionSheet = adoptNS([allocMPMediaControlsViewControllerInstance() init]);

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

#endif

#endif // PLATFORM(IOS) && ENABLE(AIRPLAY_PICKER)
