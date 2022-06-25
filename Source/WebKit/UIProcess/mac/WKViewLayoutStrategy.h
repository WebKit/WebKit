/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef WKViewLayoutStrategy_h
#define WKViewLayoutStrategy_h

#if PLATFORM(MAC)

#import "WKLayoutMode.h"
#import <wtf/NakedPtr.h>
#import <wtf/NakedRef.h>

namespace WebKit {
class WebPageProxy;
class WebViewImpl;
}

@class NSView;

@interface WKViewLayoutStrategy : NSObject {
@package
    NakedPtr<WebKit::WebPageProxy> _page;
    NakedPtr<WebKit::WebViewImpl> _webViewImpl;
    NSView *_view;

    WKLayoutMode _layoutMode;
    unsigned _frameSizeUpdatesDisabledCount;
}

+ (instancetype)layoutStrategyWithPage:(NakedRef<WebKit::WebPageProxy>)page view:(NSView *)view viewImpl:(NakedRef<WebKit::WebViewImpl>)webViewImpl mode:(WKLayoutMode)mode;

- (void)invalidate;

- (void)enableFrameSizeUpdates;
- (void)disableFrameSizeUpdates;
- (BOOL)frameSizeUpdatesDisabled;

- (void)didChangeViewScale;
- (void)willStartLiveResize;
- (void)didEndLiveResize;
- (void)didChangeFrameSize;
- (void)willChangeLayoutStrategy;

@property (nonatomic, readonly) WKLayoutMode layoutMode;

@end

#endif // PLATFORM(MAC)

#endif // WKViewLayoutStrategy_h
