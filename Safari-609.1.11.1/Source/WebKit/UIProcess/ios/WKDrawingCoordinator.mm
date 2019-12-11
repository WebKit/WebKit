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
#import "WKDrawingView.h"
#import "WKWebView.h"
#import <WebKitAdditions/WKDrawingCoordinatorAdditions.mm>
#import <wtf/RetainPtr.h>

#import "PencilKitSoftLink.h"

@interface WKDrawingCoordinator () <WKInkPickerDelegate>
@end

@implementation WKDrawingCoordinator {
    __weak WKContentView *_contentView;

    RetainPtr<WKInkPicker> _inkPicker;

    WebCore::GraphicsLayer::EmbeddedViewID _focusedEmbeddedViewID;
}

- (instancetype)initWithContentView:(WKContentView *)contentView
{
    self = [super init];
    if (!self)
        return nil;

    _contentView = contentView;

    return self;
}

- (PKInk *)currentInk
{
    return [_inkPicker currentInk];
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

- (NSUndoManager *)undoManagerForInkPicker:(WKInkPicker *)inkPicker
{
    return [_contentView undoManager];
}

- (UIView *)containingViewForInkPicker:(WKInkPicker *)inkPicker
{
    return _contentView;
}

- (void)inkPickerDidToggleRuler:(WKInkPicker *)inkPicker
{
    _rulerEnabled = !_rulerEnabled;
    [self._focusedDrawingView didChangeRulerState:_rulerEnabled];
}

- (void)inkPickerDidChangeInk:(WKInkPicker *)inkPicker
{
    [self._focusedDrawingView didChangeInk:self.currentInk];
}

- (void)installInkPickerForDrawing:(WebCore::GraphicsLayer::EmbeddedViewID)embeddedViewID
{
    _focusedEmbeddedViewID = embeddedViewID;

    if (!_inkPicker)
        _inkPicker = adoptNS([[WKInkPicker alloc] initWithDelegate:self]);
    [_inkPicker show];

    // When focused, push the ruler state down to the canvas so that it doesn't get out of sync
    // and early-return from later changes.
    [self._focusedDrawingView didChangeRulerState:_rulerEnabled];
}

- (void)uninstallInkPicker
{
    [_inkPicker hide];
    [self._focusedDrawingView didChangeRulerState:NO];
    _focusedEmbeddedViewID = 0;
}

@end

#endif // HAVE(PENCILKIT)
