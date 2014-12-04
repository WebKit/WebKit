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
#import "_WKThumbnailViewInternal.h"

#if WK_API_ENABLED

#if PLATFORM(MAC)

#import "ImageOptions.h"
#import "WKAPICast.h"
#import "WKView.h"
#import "WKViewInternal.h"
#import "WebPageProxy.h"

// FIXME: Make it possible to leave a snapshot of the content presented in the WKView while the thumbnail is live.
// FIXME: Don't make new speculative tiles while thumbnailed.
// FIXME: Hide scrollbars in the thumbnail.
// FIXME: We should re-use existing tiles for unparented views, if we have them (we need to know if they've been purged; if so, repaint at scaled-down size).
// FIXME: We should switch to the low-resolution scale if a view we have high-resolution tiles for repaints.

using namespace WebCore;
using namespace WebKit;

@implementation _WKThumbnailView {
    RetainPtr<WKView> _wkView;
    WebPageProxy* _webPageProxy;

    BOOL _originalMayStartMediaWhenInWindow;
    BOOL _originalSourceViewIsInWindow;

    BOOL _snapshotWasDeferred;
    double _lastSnapshotScale;
}

@synthesize _waitingForSnapshot = _waitingForSnapshot;

- (instancetype)initWithFrame:(NSRect)frame fromWKView:(WKView *)wkView
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    self.wantsLayer = YES;
    self.layer.backgroundColor = [NSColor whiteColor].CGColor;

    _wkView = wkView;
    _webPageProxy = toImpl([_wkView pageRef]);
    _scale = 1;
    _lastSnapshotScale = NAN;

    _originalMayStartMediaWhenInWindow = _webPageProxy->mayStartMediaWhenInWindow();
    _originalSourceViewIsInWindow = !![_wkView window];

    return self;
}

- (void)_viewWasUnparented
{
    [_wkView _setThumbnailView:nil];
    [_wkView _setIgnoresAllEvents:NO];

    self.layer.contents = nil;
    _lastSnapshotScale = NAN;

    _webPageProxy->setMayStartMediaWhenInWindow(_originalMayStartMediaWhenInWindow);
}

- (void)_viewWasParented
{
    if ([_wkView _thumbnailView])
        return;

    if (!_originalSourceViewIsInWindow)
        _webPageProxy->setMayStartMediaWhenInWindow(false);

    [self _requestSnapshotIfNeeded];
    [_wkView _setThumbnailView:self];
    [_wkView _setIgnoresAllEvents:YES];
}

- (void)_requestSnapshotIfNeeded
{
    if (self.layer.contents && _lastSnapshotScale == _scale)
        return;

    if (_waitingForSnapshot) {
        _snapshotWasDeferred = YES;
        return;
    }

    _waitingForSnapshot = YES;

    RetainPtr<_WKThumbnailView> thumbnailView = self;
    IntRect snapshotRect(IntPoint(), _webPageProxy->viewSize() - IntSize(0, _webPageProxy->topContentInset()));
    SnapshotOptions options = SnapshotOptionsInViewCoordinates;
    IntSize bitmapSize = snapshotRect.size();
    bitmapSize.scale(_scale * _webPageProxy->deviceScaleFactor());
    _lastSnapshotScale = _scale;
    _webPageProxy->takeSnapshot(snapshotRect, bitmapSize, options, [thumbnailView](const ShareableBitmap::Handle& imageHandle, CallbackBase::Error) {
        RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(imageHandle, SharedMemory::ReadOnly);
        RetainPtr<CGImageRef> cgImage = bitmap ? bitmap->makeCGImage() : nullptr;
        [thumbnailView _didTakeSnapshot:cgImage.get()];
    });
}

- (void)_didTakeSnapshot:(CGImageRef)image
{
    _waitingForSnapshot = NO;
    self.layer.sublayers = @[];
    self.layer.contentsGravity = kCAGravityResizeAspectFill;
    self.layer.contents = (id)image;

    // If we got a scale change while snapshotting, we'll take another snapshot once the first one returns.
    if (_snapshotWasDeferred) {
        _snapshotWasDeferred = NO;
        [self _requestSnapshotIfNeeded];
    }
}

- (void)viewDidMoveToWindow
{
    if (self.window)
        [self _viewWasParented];
    else
        [self _viewWasUnparented];
}

- (void)setScale:(CGFloat)scale
{
    if (_scale == scale)
        return;

    _scale = scale;

    [self _requestSnapshotIfNeeded];

    self.layer.sublayerTransform = CATransform3DMakeScale(_scale, _scale, 1);
}

// This should be removed when all clients go away; it is always YES now.
- (void)setUsesSnapshot:(BOOL)usesSnapshot
{
}

- (BOOL)usesSnapshot
{
    return YES;
}

- (void)_setThumbnailLayer:(CALayer *)layer
{
    self.layer.sublayers = layer ? @[ layer ] : @[ ];
}

- (CALayer *)_thumbnailLayer
{
    if (!self.layer.sublayers.count)
        return nil;

    return [self.layer.sublayers objectAtIndex:0];
}

@end

#endif // PLATFORM(MAC)

#endif // WK_API_ENABLED
