/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "WKFormColorControl.h"

#if ENABLE(INPUT_TYPE_COLOR) && PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import "WKContentView.h"
#import "WKFormColorPicker.h"
#import "WKFormPopover.h"

#pragma mark - WKColorPopover

static const CGFloat colorPopoverWidth = 290;
static const CGFloat colorPopoverCornerRadius = 9;

@interface WKColorPopover : WKFormRotatingAccessoryPopover<WKFormControl> {
    RetainPtr<NSObject<WKFormControl>> _innerControl;
}

- (instancetype)initWithView:(WKContentView *)view;
@end

@implementation WKColorPopover

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super initWithView:view]))
        return nil;

    _innerControl = adoptNS([[WKColorPicker alloc] initWithView:view]);

    RetainPtr<UIViewController> popoverViewController = adoptNS([[UIViewController alloc] init]);
    RetainPtr<UIView> controlContainerView = adoptNS([[UIView alloc] initWithFrame:CGRectMake(0, 0, colorPopoverWidth, colorPopoverWidth)]);

    UIView *controlView = [_innerControl controlView];
    [controlView setCenter:[controlContainerView center]];
    [controlView.layer setCornerRadius:colorPopoverCornerRadius];
    [controlView setClipsToBounds:YES];
    [controlContainerView addSubview:controlView];

    [popoverViewController setView:controlContainerView.get()];
    [popoverViewController setPreferredContentSize:[controlContainerView size]];

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr<UIPopoverController> controller = adoptNS([[UIPopoverController alloc] initWithContentViewController:popoverViewController.get()]);
    ALLOW_DEPRECATED_DECLARATIONS_END
    [self setPopoverController:controller.get()];

    return self;
}

- (UIView *)controlView
{
    return nil;
}

- (void)controlBeginEditing
{
    [self presentPopoverAnimated:NO];
}

- (void)controlEndEditing
{
    [self dismissPopoverAnimated:NO];
}

@end

#pragma mark - WKFormColorControl

@implementation WKFormColorControl

- (instancetype)initWithView:(WKContentView *)view
{
    RetainPtr<NSObject <WKFormControl>> control;
    if (currentUserInterfaceIdiomIsPad())
        control = adoptNS([[WKColorPopover alloc] initWithView:view]);
    else
        control = adoptNS([[WKColorPicker alloc] initWithView:view]);
    return [super initWithView:view control:WTFMove(control)];
}

@end

#endif // ENABLE(INPUT_TYPE_COLOR) && PLATFORM(IOS_FAMILY)
