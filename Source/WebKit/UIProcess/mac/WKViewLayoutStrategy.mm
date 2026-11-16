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

+ (instancetype)layoutStrategyWithPage:(std::reference_wrapper<WebKit::WebPageProxy>)page view:(NSView *)view viewImpl:(std::reference_wrapper<WebKit::WebViewImpl>)webViewImpl mode:(WKLayoutMode)mode
{
    RetainPtr<WKViewLayoutStrategy> strategy;

    switch (mode) {
    case kWKLayoutModeFixedSize:
        strategy = adoptNS([[WKViewFixedSizeLayoutStrategy alloc] initWithPage:page view:view viewImpl:webViewImpl mode:mode]);
        break;
    case kWKLayoutModeDynamicSizeComputedFromViewScale:
        strategy = adoptNS([[WKViewDynamicSizeComputedFromViewScaleLayoutStrategy alloc] initWithPage:page view:view viewImpl:webViewImpl mode:mode]);
        break;
    case kWKLayoutModeDynamicSizeComputedFromMinimumDocumentSize:
        strategy = adoptNS([[WKViewDynamicSizeComputedFromMinimumDocumentSizeLayoutStrategy alloc] initWithPage:page view:view viewImpl:webViewImpl mode:mode]);
        break;
    case kWKLayoutModeViewSize:
    default:
        strategy = adoptNS([[WKViewViewSizeLayoutStrategy alloc] initWithPage:page view:view viewImpl:webViewImpl mode:mode]);
        break;
    }

    [strategy updateLayout];

    return strategy.autorelease();
}

- (instancetype)initWithPage:(std::reference_wrapper<WebKit::WebPageProxy>)page view:(NSView *)view viewImpl:(std::reference_wrapper<WebKit::WebViewImpl>)webViewImpl mode:(WKLayoutMode)mode
{
    self = [super init];

    if (!self)
        return nil;

    _page = page.get();
    _webViewImpl = webViewImpl.get();
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
}

- (void)enableFrameSizeUpdates
{
}

- (BOOL)frameSizeUpdatesDisabled
{
    return NO;
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
    CheckedRef webViewImpl = *_webViewImpl;
    if (webViewImpl->clipsToVisibleRect())
        webViewImpl->updateViewExposedRect();
    webViewImpl->setDrawingAreaSize(NSSizeToCGSize(_view.get().get().frame.size));
}

- (void)willChangeLayoutStrategy
{
}

@end

@implementation WKViewViewSizeLayoutStrategy

- (instancetype)initWithPage:(std::reference_wrapper<WebKit::WebPageProxy>)page view:(NSView *)view viewImpl:(std::reference_wrapper<WebKit::WebViewImpl>)webViewImpl mode:(WKLayoutMode)mode
{
    self = [super initWithPage:page view:view viewImpl:webViewImpl mode:mode];

    if (!self)
        return nil;

    Ref { page.get() }->setUseFixedLayout(false);

    return self;
}

- (void)updateLayout
{
}

@end

@implementation WKViewFixedSizeLayoutStrategy

- (instancetype)initWithPage:(std::reference_wrapper<WebKit::WebPageProxy>)page view:(NSView *)view viewImpl:(std::reference_wrapper<WebKit::WebViewImpl>)webViewImpl mode:(WKLayoutMode)mode
{
    self = [super initWithPage:page view:view viewImpl:webViewImpl mode:mode];

    if (!self)
        return nil;

    Ref { page.get() }->setUseFixedLayout(true);

    return self;
}

- (void)updateLayout
{
}

@end

@implementation WKViewDynamicSizeComputedFromViewScaleLayoutStrategy

- (instancetype)initWithPage:(std::reference_wrapper<WebKit::WebPageProxy>)page view:(NSView *)view viewImpl:(std::reference_wrapper<WebKit::WebViewImpl>)webViewImpl mode:(WKLayoutMode)mode
{
    self = [super initWithPage:page view:view viewImpl:webViewImpl mode:mode];

    if (!self)
        return nil;

    Ref { page.get() }->setUseFixedLayout(true);

    return self;
}

- (void)updateLayout
{
    CGFloat inverseScale = 1 / _page->viewScaleFactor();
    RetainPtr view = _view.get();
    CheckedRef { *_webViewImpl }->setFixedLayoutSize(CGSizeMake(view.get().frame.size.width * inverseScale, view.get().frame.size.height * inverseScale));
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

- (instancetype)initWithPage:(std::reference_wrapper<WebKit::WebPageProxy>)page view:(NSView *)view viewImpl:(std::reference_wrapper<WebKit::WebViewImpl>)webViewImpl mode:(WKLayoutMode)mode
{
    self = [super initWithPage:page view:view viewImpl:webViewImpl mode:mode];

    if (!self)
        return nil;

    Ref { *_page }->setShouldScaleViewToFitDocument(true);

    return self;
}

- (void)updateLayout
{
}

- (void)willChangeLayoutStrategy
{
    RefPtr page = _page.get();
    page->setShouldScaleViewToFitDocument(false);
    page->scaleView(1);
}

@end

#endif // PLATFORM(MAC)
