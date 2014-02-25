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

#import "WKContentViewInteraction.h"
#import <UIKit/UIActionSheet_Private.h>
#import <UIKit/UIWindow_Private.h>

@implementation WKActionSheet {
    id <WKActionSheetDelegate> _sheetDelegate;
    id <UIActionSheetDelegate> _delegateWhileRotating;
    WKContentView *_view;
    UIPopoverArrowDirection _arrowDirections;
    BOOL _isRotating;
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

    if (UI_USER_INTERFACE_IDIOM() != UIUserInterfaceIdiomPhone) {
        [self presentFromRect:presentationRect
                       inView:view
                    direction:_arrowDirections
    allowInteractionWithViews:nil
              backgroundStyle:UIPopoverBackgroundStyleDefault
                     animated:YES];
    } else
        [self showInView:view]; 

    return YES;
}

- (void)doneWithSheet
{
    _delegateWhileRotating = nil;

    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(didRotate) object:nil];
}

#pragma mark - Rotation handling code

- (void)willRotate
{
    ASSERT(UI_USER_INTERFACE_IDIOM() != UIUserInterfaceIdiomPhone);

    if (_isRotating)
        return;

    // Clear the delegate so that the method:
    // - (void)actionSheet:(UIActionSheet *)actionSheet didDismissWithButtonIndex:(NSInteger)buttonIndex
    // is not called when the ActionSheet is dismissed below. Delegate is re-instated after rotation.
    _delegateWhileRotating = [self delegate];
    self.delegate = nil;
    _isRotating = YES;

    [self dismissWithClickedButtonIndex:[self cancelButtonIndex] animated:NO];
}

- (void)updateSheetPosition
{
    ASSERT(UI_USER_INTERFACE_IDIOM() != UIUserInterfaceIdiomPhone);

    if (!_isRotating)
        return;

    _isRotating = NO;
    
    CGRect presentationRect = [_sheetDelegate presentationRectInHostViewForSheet];
    if (!CGRectIsEmpty(presentationRect)) {
        // Re-present the popover only if we are still pointing to content onscreen.
        CGRect intersection = CGRectIntersection([[_sheetDelegate hostViewForSheet] bounds], presentationRect);
        if (!CGRectIsEmpty(intersection)) {
            self.delegate = _delegateWhileRotating;
            [self presentSheetFromRect:intersection];
            return;
        }
    }
    
    // Cancel the sheet as there is either no view or the content has now moved off screen.
    [_delegateWhileRotating actionSheet:self clickedButtonAtIndex:[self cancelButtonIndex]];
}

- (void)didRotate
{
    ASSERT(UI_USER_INTERFACE_IDIOM() != UIUserInterfaceIdiomPhone);

    [_view _updatePositionInformation];
}

@end
