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

#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WebPageProxy.h"
#import <MediaPlayer/MPAVItem.h>
#import <MediaPlayer/MPAVRoutingController.h>
#import <MediaPlayer/MPAudioVideoRoutingPopoverController.h>
#import <MediaPlayer/MPAudioVideoRoutingActionSheet.h>
#import <WebCore/SoftLinking.h>
#import <UIKit/UIApplication_Private.h>
#import <UIKit/UIWindow_Private.h>
#import <wtf/RetainPtr.h>

SOFT_LINK_FRAMEWORK(MediaPlayer)
SOFT_LINK_CLASS(MediaPlayer, MPAVRoutingController)
SOFT_LINK_CLASS(MediaPlayer, MPAudioVideoRoutingPopoverController)
SOFT_LINK_CLASS(MediaPlayer, MPAudioVideoRoutingActionSheet)

using namespace WebKit;

@implementation WKAirPlayRoutePicker {
    RetainPtr<MPAVRoutingController> _routingController;
    RetainPtr<MPAudioVideoRoutingPopoverController> _popoverController;  // iPad
    RetainPtr<MPAudioVideoRoutingActionSheet> _actionSheet;              // iPhone
    __weak WKContentView* _view;   // Weak reference.
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
    [_actionSheet dismissWithClickedButtonIndex:[_actionSheet cancelButtonIndex] animated:YES];
    [self _dismissAirPlayRoutePickerIPad];

    [super dealloc];
}

- (void)popoverControllerDidDismissPopover:(UIPopoverController *)popoverController
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

    _popoverController = adoptNS([[getMPAudioVideoRoutingPopoverControllerClass() alloc] initWithType:itemType]);
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

    _actionSheet = adoptNS([[getMPAudioVideoRoutingActionSheetClass() alloc] initWithType:itemType]);

    [_actionSheet
        showWithValidInterfaceOrientationMaskBlock:^UIInterfaceOrientationMask {
            return UIInterfaceOrientationMaskPortrait;
        }
        completionHandler:^{
            _routingController = nil;
            _actionSheet = nil;
        }
     ];
}

- (void)show:(BOOL)hasVideo fromRect:(CGRect)elementRect
{
    _routingController = adoptNS([[getMPAVRoutingControllerClass() alloc] initWithName:@"WebKit2 - HTML media element showing AirPlay route picker"]);
    [_routingController setDiscoveryMode:MPRouteDiscoveryModeDetailed];

    MPAVItemType itemType = hasVideo ? MPAVItemTypeVideo : MPAVItemTypeAudio;
    if (UICurrentUserInterfaceIdiomIsPad())
        [self showAirPlayPickerIPad:itemType fromRect:elementRect];
    else
        [self showAirPlayPickerIPhone:itemType];
}

@end

#endif // PLATFORM(IOS)
