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
#import "WKInkPickerControl.h"

#if HAVE(PENCILKIT)

#import "WKDrawingView.h"

#import "PencilKitSoftLink.h"

@interface WKInkPickerView : UIView <PKInlineInkPickerDelegate>
@end

@implementation WKInkPickerView {
    RetainPtr<PKInlineInkPicker> _inlinePicker;
    RetainPtr<WKDrawingView> _drawingView;
}

- (instancetype)initWithFrame:(CGRect)frame drawingView:(WKDrawingView *)drawingView
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;

    _inlinePicker = adoptNS([WebKit::allocPKInlineInkPickerInstance() init]);
    [_inlinePicker setSelectedInk:[drawingView canvasView].ink animated:NO];
    [_inlinePicker addTarget:self action:@selector(didPickInk) forControlEvents:UIControlEventValueChanged];
    [_inlinePicker setDelegate:self];

    _drawingView = drawingView;
    [self addSubview:_inlinePicker.get()];

    return self;
}

- (void)didPickInk
{
    [_drawingView canvasView].ink = [_inlinePicker selectedInk];
}

- (void)inlineInkPickerDidToggleRuler:(PKInlineInkPicker *)inlineInkPicker
{
    [_drawingView canvasView].rulerEnabled = ![_drawingView canvasView].rulerEnabled;
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
    return [UIViewController _viewControllerForFullScreenPresentationFromView:_drawingView.get()];
}

@end

@implementation WKInkPickerControl {
    RetainPtr<WKInkPickerView> _picker;
    RetainPtr<WKDrawingView> _drawingView;
}

- (instancetype)initWithDrawingView:(WKDrawingView *)drawingView
{
    self = [super init];
    if (!self)
        return nil;

    _drawingView = drawingView;

    return self;
}

- (void)beginEditing
{

}

- (void)endEditing
{

}

- (UIView *)assistantView
{
    if (!_picker)
        _picker = adoptNS([[WKInkPickerView alloc] initWithFrame:CGRectZero drawingView:_drawingView.get()]);
    [_picker sizeToFit];
    return _picker.get();
}

@end

#endif // HAVE(PENCILKIT)
