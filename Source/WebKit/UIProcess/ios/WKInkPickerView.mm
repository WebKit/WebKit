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
#import "WKInkPickerView.h"

#if HAVE(PENCILKIT)

#import "WKContentViewInteraction.h"
#import "WKDrawingCoordinator.h"

#import "PencilKitSoftLink.h"

@interface WKInkPickerView () <PKInlineInkPickerDelegate>
@end

@implementation WKInkPickerView {
    RetainPtr<PKInlineInkPicker> _inlinePicker;
    __weak WKContentView *_contentView;
}

- (instancetype)initWithContentView:(WKContentView *)contentView
{
    self = [super initWithFrame:CGRectZero];
    if (!self)
        return nil;

    _inlinePicker = adoptNS([WebKit::allocPKInlineInkPickerInstance() init]);
    [_inlinePicker setDelegate:self];

    _contentView = contentView;
    [self addSubview:_inlinePicker.get()];

    return self;
}

- (void)didPickInk
{
    [_contentView._drawingCoordinator didChangeInk:[_inlinePicker selectedInk]];
}

- (void)inlineInkPickerDidToggleRuler:(PKInlineInkPicker *)inlineInkPicker
{
    _rulerEnabled = !_rulerEnabled;
    [_contentView._drawingCoordinator didChangeRulerState:_rulerEnabled];
}

- (void)inlineInkPicker:(PKInlineInkPicker *)inlineInkPicker didSelectTool:(PKInkIdentifier)identifer
{
    [self didPickInk];
}

- (void)inlineInkPicker:(PKInlineInkPicker *)inlineInkPicker didSelectColor:(UIColor *)color
{
    [self didPickInk];
}

- (CGSize)inkPickerSize
{
    CGSize keyboardSize = [UIKeyboard defaultSizeForInterfaceOrientation:[UIApp interfaceOrientation]];
    return [_inlinePicker sizeThatFits:keyboardSize];
}

- (void)layoutSubviews
{
    CGSize pickerSize = self.inkPickerSize;
    [_inlinePicker setFrame:CGRectMake(CGRectGetMidX(self.bounds) - (pickerSize.width / 2), 0, pickerSize.width, pickerSize.height)];
}

- (CGSize)sizeThatFits:(CGSize)size
{
    CGSize keyboardSize = [UIKeyboard defaultSizeForInterfaceOrientation:[UIApp interfaceOrientation]];
    return CGSizeMake(keyboardSize.width, self.inkPickerSize.height);
}

- (UIViewController *)viewControllerForPopoverPresentationFromInlineInkPicker:(PKInlineInkPicker *)inlineInkPicker
{
    return [UIViewController _viewControllerForFullScreenPresentationFromView:_contentView];
}

- (void)setInk:(PKInk *)ink
{
    [_inlinePicker setSelectedInk:ink];
}

- (PKInk *)ink
{
    return [_inlinePicker selectedInk];
}

@end

#endif // HAVE(PENCILKIT)
