/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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
#import "WKViewLayoutStrategy.h"

#if PLATFORM(MAC)

#import "WebPageProxy.h"
#import "WebViewImpl.h"
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/MachSendRight.h>

@interface WKViewViewSizeLayoutStrategy : WKViewLayoutStrategy
@end

@interface WKViewFixedSizeLayoutStrategy : WKViewLayoutStrategy
@end

@interface WKViewDynamicSizeComputedFromViewScaleLayoutStrategy : WKViewLayoutStrategy
@end

@interface WKViewDynamicSizeComputedFromMinimumDocumentSizeLayoutStrategy : WKViewLayoutStrategy
@end

@implementation WKViewLayoutStrategy

+ (instancetype)layoutStrategyWithPage:(NakedRef<WebKit::WebPageProxy>)page view:(NSView *)view viewImpl:(NakedRef<WebKit::WebViewImpl>)webViewImpl mode:(WKLayoutMode)mode
{
    RetainPtr<WKViewLayoutStrategy> strategy;

    switch (mode) {
    case kWKLayoutModeFixedSize:
        strategy = adoptNS([[WKViewFixedSizeLayoutStrategy alloc] initWithPage:page.copyRef() view:view viewImpl:webViewImpl.copyRef() mode:mode]);
        break;
    case kWKLayoutModeDynamicSizeComputedFromViewScale:
        strategy = adoptNS([[WKViewDynamicSizeComputedFromViewScaleLayoutStrategy alloc] initWithPage:page.copyRef() view:view viewImpl:webViewImpl.copyRef() mode:mode]);
        break;
    case kWKLayoutModeDynamicSizeComputedFromMinimumDocumentSize:
        strategy = adoptNS([[WKViewDynamicSizeComputedFromMinimumDocumentSizeLayoutStrategy alloc] initWithPage:page.copyRef() view:view viewImpl:webViewImpl.copyRef() mode:mode]);
        break;
    case kWKLayoutModeViewSize:
    default:
        strategy = adoptNS([[WKViewViewSizeLayoutStrategy alloc] initWithPage:page.copyRef() view:view viewImpl:webViewImpl.copyRef() mode:mode]);
        break;
    }

    [strategy updateLayout];

    return strategy.autorelease();
}

- (instancetype)initWithPage:(NakedRef<WebKit::WebPageProxy>)page view:(NSView *)view viewImpl:(NakedRef<WebKit::WebViewImpl>)webViewImpl mode:(WKLayoutMode)mode
{
    self = [super init];

    if (!self)
        return nil;

    _page = page.ptr();
    _webViewImpl = webViewImpl.ptr();
    _view = view;
    _layoutMode = mode;

    return self;
}

- (void)invalidate
{
    _page = nullptr;
    _webViewImpl = nullptr;
    _view = nil;
}

- (WKLayoutMode)layoutMode
{
    return _layoutMode;
}

- (void)updateLayout
{
}

- (void)disableFrameSizeUpdates
{
    _frameSizeUpdatesDisabledCount++;
}

- (void)enableFrameSizeUpdates
{
    if (!_frameSizeUpdatesDisabledCount)
        return;

    if (!(--_frameSizeUpdatesDisabledCount))
        [self didChangeFrameSize];
}

- (BOOL)frameSizeUpdatesDisabled
{
    return _frameSizeUpdatesDisabledCount > 0;
}

- (void)didChangeViewScale
{
}

- (void)willStartLiveResize
{
}

- (void)didEndLiveResize
{
}

- (void)didChangeFrameSize
{
    if ([self frameSizeUpdatesDisabled])
        return;

    if (_webViewImpl->clipsToVisibleRect())
        _webViewImpl->updateViewExposedRect();
    _webViewImpl->setDrawingAreaSize(NSSizeToCGSize(_view.frame.size));
}

- (void)willChangeLayoutStrategy
{
}

@end

@implementation WKViewViewSizeLayoutStrategy

- (instancetype)initWithPage:(NakedRef<WebKit::WebPageProxy>)page view:(NSView *)view viewImpl:(NakedRef<WebKit::WebViewImpl>)webViewImpl mode:(WKLayoutMode)mode
{
    self = [super initWithPage:page.copyRef() view:view viewImpl:webViewImpl.copyRef() mode:mode];

    if (!self)
        return nil;

    page->setUseFixedLayout(false);

    return self;
}

- (void)updateLayout
{
}

@end

@implementation WKViewFixedSizeLayoutStrategy

- (instancetype)initWithPage:(NakedRef<WebKit::WebPageProxy>)page view:(NSView *)view viewImpl:(NakedRef<WebKit::WebViewImpl>)webViewImpl mode:(WKLayoutMode)mode
{
    self = [super initWithPage:page.copyRef() view:view viewImpl:webViewImpl.copyRef() mode:mode];

    if (!self)
        return nil;

    page->setUseFixedLayout(true);

    return self;
}

- (void)updateLayout
{
}

@end

@implementation WKViewDynamicSizeComputedFromViewScaleLayoutStrategy

- (instancetype)initWithPage:(NakedRef<WebKit::WebPageProxy>)page view:(NSView *)view viewImpl:(NakedRef<WebKit::WebViewImpl>)webViewImpl mode:(WKLayoutMode)mode
{
    self = [super initWithPage:page.copyRef() view:view viewImpl:webViewImpl.copyRef() mode:mode];

    if (!self)
        return nil;

    page->setUseFixedLayout(true);

    return self;
}

- (void)updateLayout
{
    CGFloat inverseScale = 1 / _page->viewScaleFactor();
    _webViewImpl->setFixedLayoutSize(CGSizeMake(_view.frame.size.width * inverseScale, _view.frame.size.height * inverseScale));
}

- (void)didChangeViewScale
{
    [super didChangeViewScale];

    [self updateLayout];
}

- (void)didChangeFrameSize
{
    [super didChangeFrameSize];

    if (self.frameSizeUpdatesDisabled)
        return;

    [self updateLayout];
}

@end

@implementation WKViewDynamicSizeComputedFromMinimumDocumentSizeLayoutStrategy

- (instancetype)initWithPage:(NakedRef<WebKit::WebPageProxy>)page view:(NSView *)view viewImpl:(NakedRef<WebKit::WebViewImpl>)webViewImpl mode:(WKLayoutMode)mode
{
    self = [super initWithPage:page.copyRef() view:view viewImpl:webViewImpl.copyRef() mode:mode];

    if (!self)
        return nil;

    _page->setShouldScaleViewToFitDocument(true);

    return self;
}

- (void)updateLayout
{
}

- (void)willChangeLayoutStrategy
{
    _page->setShouldScaleViewToFitDocument(false);
    _page->scaleView(1);
}

@end

#endif // PLATFORM(MAC)
