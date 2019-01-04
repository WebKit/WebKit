/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "WKDrawingCoordinator.h"

#if HAVE(PENCILKIT)

#import "EditableImageController.h"
#import "WKContentViewInteraction.h"
#import "WKInkPickerView.h"
#import "WKWebView.h"
#import <wtf/RetainPtr.h>

#import "PencilKitSoftLink.h"

@interface WKDrawingCoordinator () <PKRulerHostingDelegate>
@end

@implementation WKDrawingCoordinator {
    __weak WKContentView *_contentView;

    RetainPtr<WKInkPickerView> _inkPicker;

    WebCore::GraphicsLayer::EmbeddedViewID _focusedEmbeddedViewID;
}

- (instancetype)initWithContentView:(WKContentView *)contentView
{
    self = [super init];
    if (!self)
        return nil;

    _contentView = contentView;
    _inkPicker = adoptNS([[WKInkPickerView alloc] initWithContentView:_contentView]);

    return self;
}

- (WKInkPickerView *)inkPicker
{
    return _inkPicker.get();
}

- (UIView *)rulerHostingView
{
    return _contentView;
}

- (BOOL)rulerHostWantsSharedRuler
{
    return YES;
}

- (WKDrawingView *)_focusedDrawingView
{
    WebKit::EditableImage *focusedEditableImage = _contentView.page->editableImageController().editableImage(_focusedEmbeddedViewID);
    if (!focusedEditableImage)
        return nil;
    return focusedEditableImage->drawingView.get();
}

- (void)didChangeRulerState:(BOOL)rulerEnabled
{
    [self._focusedDrawingView didChangeRulerState:rulerEnabled];
}

- (void)didChangeInk:(PKInk *)ink
{
    [self._focusedDrawingView didChangeInk:ink];
}

- (void)installInkPickerForDrawing:(WebCore::GraphicsLayer::EmbeddedViewID)embeddedViewID
{
    _focusedEmbeddedViewID = embeddedViewID;

    WKWebView *webView = _contentView->_webView;

    [_inkPicker sizeToFit];
    [_inkPicker setTranslatesAutoresizingMaskIntoConstraints:NO];
    [webView addSubview:_inkPicker.get()];

    [NSLayoutConstraint activateConstraints:@[
        [[_inkPicker heightAnchor] constraintEqualToConstant:[_inkPicker frame].size.height],
        [[_inkPicker bottomAnchor] constraintEqualToAnchor:webView.safeAreaLayoutGuide.bottomAnchor],
        [[_inkPicker leftAnchor] constraintEqualToAnchor:webView.safeAreaLayoutGuide.leftAnchor],
        [[_inkPicker rightAnchor] constraintEqualToAnchor:webView.safeAreaLayoutGuide.rightAnchor],
    ]];

    // When focused, push the ruler state down to the canvas so that it doesn't get out of sync
    // and early-return from later changes.
    [self._focusedDrawingView didChangeRulerState:[_inkPicker rulerEnabled]];
}

- (void)uninstallInkPicker
{
    [_inkPicker removeFromSuperview];
    _focusedEmbeddedViewID = 0;
}

@end

#endif // HAVE(PENCILKIT)
