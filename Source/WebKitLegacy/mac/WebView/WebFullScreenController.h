/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#if ENABLE(FULLSCREEN_API) && !PLATFORM(IOS_FAMILY)

#import <WebCore/IntPoint.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>

@class WebWindowFadeAnimation;
@class WebWindowScaleAnimation;
@class WebView;
namespace WebCore {
class Element;
class RenderBox;
class EventListener;
}

@interface WebFullScreenController : NSWindowController {
@private
    RefPtr<WebCore::Element> _element;
    WebView *_webView;
    RetainPtr<NSView> _webViewPlaceholder;
    RetainPtr<WebWindowScaleAnimation> _scaleAnimation;
    RetainPtr<WebWindowFadeAnimation> _fadeAnimation;
    RetainPtr<NSWindow> _backgroundWindow;
    NSRect _initialFrame;
    NSRect _finalFrame;
    WebCore::IntPoint _scrollPosition;
    float _savedScale;

    BOOL _isEnteringFullScreen;
    BOOL _isExitingFullScreen;
    BOOL _isFullScreen;
}

@property (readonly) NSRect initialFrame;
@property (readonly) NSRect finalFrame;

- (WebView*)webView;
- (void)setWebView:(WebView*)webView;

- (NSView*)webViewPlaceholder;

- (BOOL)isFullScreen;

- (void)setElement:(RefPtr<WebCore::Element>&&)element;
- (WebCore::Element*)element;

- (void)enterFullScreen:(NSScreen *)screen;
- (void)exitFullScreen;
- (void)close;
@end

#endif // ENABLE(FULLSCREEN_API) && !PLATFORM(IOS_FAMILY)
