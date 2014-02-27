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
#import "WKFormPopover.h"

#if PLATFORM(IOS)

#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WebPageProxy.h"
#import <UIKit/UIPeripheralHost.h>
#import <UIKit/UIWindow_Private.h>
#import <wtf/RetainPtr.h>

using namespace WebKit;

@implementation WKFormRotatingAccessoryPopover {
    WKContentView *_view;
}

- (id)initWithView:(WKContentView *)view
{
    if (!(self = [super initWithView:view]))
        return nil;
    _view = view;
    self.dismissionDelegate = self;
    return self;
}

- (void)accessoryDone
{
    [_view accessoryDone];
}

- (UIPopoverArrowDirection)popoverArrowDirections
{
    UIPopoverArrowDirection directions = UIPopoverArrowDirectionUp | UIPopoverArrowDirectionDown;
    if (UIInterfaceOrientationIsLandscape([[UIApplication sharedApplication] statusBarOrientation]) && [[UIPeripheralHost sharedInstance] isOnScreen])
        directions = UIPopoverArrowDirectionLeft | UIPopoverArrowDirectionRight;
    return directions;
}

- (void)popoverWasDismissed:(WKRotatingPopover *)popover
{
    [self accessoryDone];
}
@end

@implementation WKRotatingPopover {
    WKContentView *_view;
}

- (id)initWithView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;

    _view = view;
    self.presentationPoint = CGPointZero;

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(willRotate:) name:UIWindowWillRotateNotification object:nil];
    [center addObserver:self selector:@selector(didRotate:) name:UIWindowDidRotateNotification object:nil];

    return self;
}

- (void)dealloc
{
    _view = nil;

    [_popoverController dismissPopoverAnimated:YES];
    [_popoverController setDelegate:nil];
    self.popoverController = nil;

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center removeObserver:self name:UIWindowWillRotateNotification object:nil];
    [center removeObserver:self name:UIWindowDidRotateNotification object:nil];

    [super dealloc];
}

- (UIPopoverController *)popoverController
{
    return _popoverController.get();
}

- (void)setPopoverController:(UIPopoverController *)popoverController
{
    if (_popoverController == popoverController)
        return;

    [_popoverController setDelegate:nil];
    _popoverController = popoverController;
    [_popoverController setDelegate:self];
}

- (UIPopoverArrowDirection)popoverArrowDirections
{
    return UIPopoverArrowDirectionAny;
}

- (void)presentPopoverAnimated:(BOOL)animated
{
    UIPopoverArrowDirection directions = [self popoverArrowDirections];

    BOOL presentWithPoint = !CGPointEqualToPoint(self.presentationPoint, CGPointZero);
    if (presentWithPoint) {
        CGFloat scale = [_view page]->pageScaleFactor();
        [_popoverController presentPopoverFromRect:CGRectIntegral(CGRectMake(self.presentationPoint.x * scale, self.presentationPoint.y * scale, 1, 1))
                                            inView:_view
                          permittedArrowDirections:directions
                                          animated:animated];
    } else {
        CGRect boundingBoxOfDOMNode = _view.assistedNodeInformation.elementRect;
        [_popoverController presentPopoverFromRect:CGRectIntegral(boundingBoxOfDOMNode)
                                            inView:_view
                          permittedArrowDirections:directions
                                          animated:animated];
    }
}

- (void)dismissPopoverAnimated:(BOOL)animated
{
    [_popoverController dismissPopoverAnimated:animated];
}

- (void)willRotate:(NSNotification *)notification
{
    _isRotating = YES;
    [_popoverController dismissPopoverAnimated:NO];
}

- (void)didRotate:(NSNotification *)notification
{
    _isRotating = NO;
    [self presentPopoverAnimated:NO];
}

- (void)popoverControllerDidDismissPopover:(UIPopoverController *)popoverController
{
    if (_isRotating)
        return;

    [_dismissionDelegate popoverWasDismissed:self];
}

@end

#endif // PLATFORM(IOS)
