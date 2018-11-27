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
#import "WKDrawingView.h"

#if HAVE(PENCILKIT)

#import "EditableImageController.h"
#import "PencilKitSPI.h"
#import <wtf/OSObjectPtr.h>
#import <wtf/RetainPtr.h>

SOFT_LINK_PRIVATE_FRAMEWORK(PencilKit);
SOFT_LINK_CLASS(PencilKit, PKCanvasView);
SOFT_LINK_CLASS(PencilKit, PKImageRenderer);

@interface WKDrawingView () <PKCanvasViewDelegate>
@end

@implementation WKDrawingView {
    RetainPtr<PKCanvasView> _pencilView;

    OSObjectPtr<dispatch_queue_t> _renderQueue;
    RetainPtr<PKImageRenderer> _renderer;

    WeakPtr<WebKit::WebPageProxy> _webPageProxy;
}

- (instancetype)initWithEmbeddedViewID:(WebCore::GraphicsLayer::EmbeddedViewID)embeddedViewID webPageProxy:(WebKit::WebPageProxy&)webPageProxy
{
    self = [super initWithEmbeddedViewID:embeddedViewID];
    if (!self)
        return nil;

    _webPageProxy = makeWeakPtr(webPageProxy);

    _pencilView = adoptNS([allocPKCanvasViewInstance() initWithFrame:CGRectZero]);

    [_pencilView setFingerDrawingEnabled:NO];
    [_pencilView setUserInteractionEnabled:YES];
    [_pencilView setOpaque:NO];
    [_pencilView setDrawingDelegate:self];

    [self addSubview:_pencilView.get()];

    return self;
}

- (void)layoutSubviews
{
    if (!CGRectEqualToRect([_pencilView frame], self.bounds)) {
        [_pencilView setFrame:self.bounds];

        // The renderer is instantiated for a particular size output; if
        // the size changes, we need to re-create the renderer.
        _renderer = nil;
    }
}

- (NSData *)PNGRepresentation
{
    if (!_renderQueue)
        _renderQueue = adoptOSObject(dispatch_queue_create("com.apple.WebKit.WKDrawingView.Rendering", DISPATCH_QUEUE_SERIAL));

    if (!_renderer)
        _renderer = adoptNS([allocPKImageRendererInstance() initWithSize:self.bounds.size scale:self.window.screen.scale renderQueue:_renderQueue.get()]);

    __block RetainPtr<UIImage> resultImage;
    [_renderer renderDrawing:[_pencilView drawing] completion:^(UIImage *image) {
        resultImage = image;
    }];

    // FIXME: Ideally we would not synchronously wait for this rendering,
    // but NSFileWrapper requires data synchronously, and our clients expect
    // an NSFileWrapper to be available synchronously.
    dispatch_sync(_renderQueue.get(), ^{ });

    return UIImagePNGRepresentation(resultImage.get());
}

- (void)drawingDidChange:(PKCanvasView *)canvasView
{
    if (!_webPageProxy)
        return;
    auto& page = *_webPageProxy;

    page.editableImageController().invalidateAttachmentForEditableImage(self.embeddedViewID);
}

@end

#endif // HAVE(PENCILKIT)
