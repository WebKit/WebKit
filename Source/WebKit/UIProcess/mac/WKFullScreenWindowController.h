/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#if ENABLE(FULLSCREEN_API)

#import "GenericCallback.h"
#import <wtf/RetainPtr.h>

namespace WebKit { 
class LayerTreeContext;
class WebPageProxy;
class WKFullScreenWindowControllerVideoFullscreenModelClient;
}

namespace WebCore {
class IntRect;
}

@class WKView;
@class WebCoreFullScreenPlaceholderView;

typedef enum FullScreenState : NSInteger FullScreenState;

@interface WKFullScreenWindowController : NSWindowController<NSWindowDelegate> {
@private
    NSView *_webView; // Cannot be retained, see <rdar://problem/14884666>.
    WebKit::WebPageProxy* _page;
    RetainPtr<WebCoreFullScreenPlaceholderView> _webViewPlaceholder;
    RetainPtr<NSView> _exitPlaceholder;
    RetainPtr<NSView> _clipView;
    RetainPtr<NSView> _backgroundView;
    NSRect _initialFrame;
    NSRect _finalFrame;
    RetainPtr<NSTimer> _watchdogTimer;
    RetainPtr<NSArray> _savedConstraints;

    bool _requestedExitFullScreen;
    FullScreenState _fullScreenState;

    double _savedScale;
    RefPtr<WebKit::VoidCallback> _repaintCallback;
    std::unique_ptr<WebKit::WKFullScreenWindowControllerVideoFullscreenModelClient> _videoFullscreenClient;
    float _savedTopContentInset;
}

@property (readonly) NSRect initialFrame;
@property (readonly) NSRect finalFrame;
@property (assign) NSArray *savedConstraints;

- (id)initWithWindow:(NSWindow *)window webView:(NSView *)webView page:(WebKit::WebPageProxy&)page;

- (WebCoreFullScreenPlaceholderView*)webViewPlaceholder;

- (BOOL)isFullScreen;

- (void)enterFullScreen:(NSScreen *)screen;
- (void)exitFullScreen;
- (void)requestExitFullScreen;
- (void)close;
- (void)beganEnterFullScreenWithInitialFrame:(NSRect)initialFrame finalFrame:(NSRect)finalFrame;
- (void)beganExitFullScreenWithInitialFrame:(NSRect)initialFrame finalFrame:(NSRect)finalFrame;

- (void)videoControlsManagerDidChange;
- (void)didEnterPictureInPicture;

@end

#endif
