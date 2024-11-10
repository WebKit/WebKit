/*
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
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

#import "FocusedElementInformation.h"
#import "UIKitSPI.h"
#import "UIKitUtilities.h"
#import "WKContentViewInteraction.h"
#import "WebPageProxy.h"
#import <WebCore/ColorCocoa.h>
#import <pal/system/ios/UserInterfaceIdiom.h>
#import <wtf/cocoa/VectorCocoa.h>

#pragma mark - WKColorPicker

@interface WKColorPicker : NSObject<WKFormControl, UIColorPickerViewControllerDelegate, UIPopoverPresentationControllerDelegate>
- (instancetype)initWithView:(WKContentView *)view;
- (void)selectColor:(UIColor *)color;
@end

@implementation WKColorPicker {
    __weak WKContentView *_view;

    RetainPtr<UIColorPickerViewController> _colorPickerViewController;
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;

    _view = view;

    _colorPickerViewController = adoptNS([[UIColorPickerViewController alloc] init]);
    [_colorPickerViewController setDelegate:self];
    [_colorPickerViewController setSupportsAlpha:NO];

    return self;
}

- (void)selectColor:(UIColor *)color
{
    [_colorPickerViewController setSelectedColor:color];
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [self colorPickerViewControllerDidSelectColor:_colorPickerViewController.get()];
ALLOW_DEPRECATED_DECLARATIONS_END
}

#if ENABLE(DATALIST_ELEMENT)
- (NSArray<UIColor *> *)focusedElementSuggestedColors
{
    auto& colors = _view.focusedElementInformation.suggestedColors;

    if (colors.isEmpty())
        return nil;

    return createNSArray(colors, [] (auto& color) {
        return cocoaColor(color);
    }).autorelease();
}
#endif

- (void)updateColorPickerState
{
    [_colorPickerViewController setSelectedColor:cocoaColor(_view.focusedElementInformation.colorValue).get()];
    [_colorPickerViewController setSupportsAlpha:_view.focusedElementInformation.supportsAlpha == WebKit::ColorControlSupportsAlpha::Yes && _view.page->preferences().inputTypeColorEnhancementsEnabled()];
#if ENABLE(DATALIST_ELEMENT)
    if ([_colorPickerViewController respondsToSelector:@selector(_setSuggestedColors:)])
        [_colorPickerViewController _setSuggestedColors:[self focusedElementSuggestedColors]];
#endif
}

- (void)configurePresentation
{
    [_colorPickerViewController setModalPresentationStyle:UIModalPresentationPopover];

    UIPopoverPresentationController *presentationController = [_colorPickerViewController popoverPresentationController];
    presentationController.delegate = self;
    presentationController.sourceView = _view;
    presentationController.sourceRect = CGRectIntegral(_view.focusedElementInformation.interactionRect);
}

#pragma mark WKFormControl

- (UIView *)controlView
{
    return nil;
}

- (void)controlBeginEditing
{
    [_view startRelinquishingFirstResponderToFocusedElement];

    [self updateColorPickerState];
    [self configurePresentation];

    auto presentingViewController = _view._wk_viewControllerForFullScreenPresentation;
    [presentingViewController presentViewController:_colorPickerViewController.get() animated:YES completion:nil];
}

- (void)controlUpdateEditing
{
}

- (void)controlEndEditing
{
    [_view stopRelinquishingFirstResponderToFocusedElement];
    [_colorPickerViewController dismissViewControllerAnimated:NO completion:nil];
}

#pragma mark UIPopoverPresentationControllerDelegate

- (void)presentationControllerDidDismiss:(UIPresentationController *)presentationController
{
    [_view accessoryDone];
}

#pragma mark UIColorPickerViewControllerDelegate

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)colorPickerViewControllerDidSelectColor:(UIColorPickerViewController *)viewController
{
    [_view updateFocusedElementValueAsColor:viewController.selectedColor];
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (void)colorPickerViewControllerDidFinish:(UIColorPickerViewController *)viewController
{
    [_view accessoryDone];
}

@end

#pragma mark - WKFormColorControl

@implementation WKFormColorControl

- (instancetype)initWithView:(WKContentView *)view
{
    RetainPtr<NSObject <WKFormControl>> control = adoptNS([[WKColorPicker alloc] initWithView:view]);
    self = [super initWithView:view control:WTFMove(control)];
    return self;
}

@end

@implementation WKFormColorControl (WKTesting)

- (void)selectColor:(UIColor *)color
{
    if (auto *picker = dynamic_objc_cast<WKColorPicker>(self.control))
        [picker selectColor:color];
}

@end

#endif // ENABLE(INPUT_TYPE_COLOR) && PLATFORM(IOS_FAMILY)
