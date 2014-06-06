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
#import "WKActionSheet.h"

#if PLATFORM(IOS)

#import "WKContentViewInteraction.h"
#import <UIKit/UIAlertController_Private.h>
#import <UIKit/UIWindow_Private.h>

@implementation WKActionSheet {
    id <WKActionSheetDelegate> _sheetDelegate;
    WKContentView *_view;
    UIPopoverArrowDirection _arrowDirections;
    BOOL _isRotating;
    BOOL _readyToPresentAfterRotation;

    RetainPtr<UIViewController> _presentedViewControllerWhileRotating;
    RetainPtr<id <UIPopoverPresentationControllerDelegate>> _popoverPresentationControllerDelegateWhileRotating;
}

- (id)initWithView:(WKContentView *)view
{
    self = [super init];
    if (!self)
        return nil;

    _arrowDirections = UIPopoverArrowDirectionAny;
    _view = view;

    if (UI_USER_INTERFACE_IDIOM() != UIUserInterfaceIdiomPhone) {
        // Only iPads support popovers that rotate. UIActionSheets actually block rotation on iPhone/iPod Touch
        NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
        [center addObserver:self selector:@selector(willRotate) name:UIWindowWillRotateNotification object:nil];
        [center addObserver:self selector:@selector(didRotate) name:UIWindowDidRotateNotification object:nil];
    }

    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [NSObject cancelPreviousPerformRequestsWithTarget:self];

    [super dealloc];
}

#pragma mark - Sheet presentation code

- (BOOL)presentSheet
{
    // Calculate the presentation rect just before showing.
    CGRect presentationRect = CGRectZero;
    if (UI_USER_INTERFACE_IDIOM() != UIUserInterfaceIdiomPhone) {
        presentationRect = [_sheetDelegate initialPresentationRectInHostViewForSheet];
        if (CGRectIsEmpty(presentationRect))
            return NO;
    }

    return [self presentSheetFromRect:presentationRect];
}

- (BOOL)presentSheetFromRect:(CGRect)presentationRect
{
    UIView *view = [_sheetDelegate hostViewForSheet];
    if (!view)
        return NO;

    UIViewController *presentedViewController = _presentedViewControllerWhileRotating.get() ? _presentedViewControllerWhileRotating.get() : self;
    presentedViewController.modalPresentationStyle = UIModalPresentationPopover;

    UIPopoverPresentationController *presentationController = presentedViewController.popoverPresentationController;
    presentationController.sourceView = view;
    presentationController.sourceRect = presentationRect;
    presentationController.permittedArrowDirections = _arrowDirections;

    if (_popoverPresentationControllerDelegateWhileRotating)
        presentationController.delegate = _popoverPresentationControllerDelegateWhileRotating.get();

    UIViewController *presentingViewController = [view.window rootViewController];
    [presentingViewController presentViewController:presentedViewController animated:YES completion:NULL];

    return YES;
}

- (void)doneWithSheet
{
    _presentedViewControllerWhileRotating = nil;
    _popoverPresentationControllerDelegateWhileRotating = nil;

    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_didRotateAndLayout) object:nil];
}

#pragma mark - Rotation handling code

- (void)willRotate
{
    // We want to save the view controller that is currently being presented to re-present it after rotation.
    // Here are the various possible states that we have to handle:
    // a) topViewController presenting ourselves (alertViewController) -> nominal case.
    //    There is no need to save the presented view controller, which is self.
    // b) topViewController presenting ourselves presenting a content view controller ->
    //    This happens if one of the actions in the action sheet presented a different view controller inside the popover,
    //    using a current context presentation. This is for example the case with the Data Detectors action "Add to Contacts".
    // c) topViewController presenting that content view controller directly.
    //    This happens if we were in the (b) case and then rotated the device. Since we dismiss the popover during the
    //    rotation, we take this opportunity to simplify the view controller hierarchy and simply re-present the content
    //    view controller, without re-presenting the alert controller.

    UIView *view = [_sheetDelegate hostViewForSheet];
    UIViewController *presentingViewController = view.window.rootViewController;

    // topPresentedViewController is either self (cases (a) and (b) above) or an action's view controller
    // (case (c) above).
    UIViewController *topPresentedViewController = [presentingViewController presentedViewController];

    // We only have something to do if we're showing a popover (that we have to reposition).
    // Otherwise the default UIAlertController behaviour is enough.
    if ([topPresentedViewController presentationController].presentationStyle != UIModalPresentationPopover)
        return;

    if (_isRotating)
        return;

    _isRotating = YES;
    _readyToPresentAfterRotation = NO;

    UIViewController *presentedViewController = nil;
    if ([self presentingViewController] != nil) {
        // Handle cases (a) and (b) above (we (UIAlertController) are still in the presentation hierarchy).
        // Save the view controller presented by one of the actions if there is one.
        // (In the (a) case, presentedViewController will be nil).

        presentedViewController = [self presentedViewController];
    } else {
        // Handle case (c) above.
        // The view controller that we want to save is the top presented view controller, since we
        // are not presenting it anymore.

        presentedViewController = topPresentedViewController;
    }

    _presentedViewControllerWhileRotating = presentedViewController;

    // Save the popover presentation controller's delegate, because in case (b) we're going to use
    // a different popoverPresentationController after rotation to re-present the action view controller,
    // and that action is still expecting delegate callbacks when the popover is dismissed.
    _popoverPresentationControllerDelegateWhileRotating = [topPresentedViewController popoverPresentationController].delegate;

    [presentingViewController dismissViewControllerAnimated:NO completion:^{
        [self updateSheetPosition];
    }];
}

- (void)updateSheetPosition
{
    UIViewController *presentedViewController = _presentedViewControllerWhileRotating.get() ? _presentedViewControllerWhileRotating.get() : self;

    // There are two asynchronous events which might trigger this call, and we have to wait for both of them before doing something.
    // - One runloop iteration after rotation (to let the Web content re-layout, see below)
    // - The completion of the view controller dismissal in willRotate.
    // (We cannot present something again until the dismissal is done)

    BOOL isBeingPresented = [presentedViewController presentingViewController] || [self presentingViewController];

    if (!_isRotating || !_readyToPresentAfterRotation || isBeingPresented)
        return;

    CGRect presentationRect = [_sheetDelegate initialPresentationRectInHostViewForSheet];
    BOOL wasPresentedViewControllerModal = [_presentedViewControllerWhileRotating isModalInPopover];

    if (!CGRectIsEmpty(presentationRect) || wasPresentedViewControllerModal) {
        // Re-present the popover only if we are still pointing to content onscreen, or if we can't dismiss it without losing information.
        // (if the view controller is modal)

        CGRect intersection = CGRectIntersection([[_sheetDelegate hostViewForSheet] bounds], presentationRect);
        if (!CGRectIsEmpty(intersection))
            [self presentSheetFromRect:intersection];
        else if (wasPresentedViewControllerModal)
            [self presentSheet];

        _presentedViewControllerWhileRotating = nil;
        _popoverPresentationControllerDelegateWhileRotating = nil;
    }
}

- (void)_didRotateAndLayout
{
    _isRotating = NO;
    _readyToPresentAfterRotation = YES;
    [_view _updatePositionInformation];
    [self updateSheetPosition];
}

- (void)didRotate
{
    // Handle the rotation on the next run loop interation as this
    // allows the onOrientationChange event to fire, and the element node may
    // be removed.
    // <rdar://problem/9360929> Should re-present popover after layout rather than on the next runloop

    [self performSelector:@selector(_didRotateAndLayout) withObject:nil afterDelay:0];
}

@end

#endif // PLATFORM(IOS)
