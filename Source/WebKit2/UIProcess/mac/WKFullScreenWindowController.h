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

#import <Cocoa/Cocoa.h>
#import <wtf/RetainPtr.h>

namespace WebKit { 
class LayerTreeContext;
}

namespace WebCore {
class IntRect;
}

@class WKView;

@interface WKFullScreenWindowController : NSWindowController {
@private
    WKView *_webView;
    RetainPtr<NSView> _webViewPlaceholder;
    RetainPtr<NSView> _layerViewPlaceholder;
    RetainPtr<NSView> _layerHostingView;
    
    BOOL _isEnteringFullScreen;
    BOOL _isExitingFullScreen;
    BOOL _isFullScreen;
    BOOL _isWindowLoaded;
    BOOL _forceDisableAnimation;
    BOOL _isPlaying;
    CGRect _initialFrame;    
    uint32_t _idleDisplaySleepAssertion;
    uint32_t _idleSystemSleepAssertion;
    NSTimer *_tickleTimer;
}

- (WKView*)webView;
- (void)setWebView:(WKView*)webView;

- (void)enterFullScreen:(NSScreen *)screen;
- (void)exitFullScreen;
- (void)beganEnterFullScreenAnimation;
- (void)beganExitFullScreenAnimation;
- (void)finishedEnterFullScreenAnimation:(bool)completed;
- (void)finishedExitFullScreenAnimation:(bool)completed;
- (void)enterAcceleratedCompositingMode:(const WebKit::LayerTreeContext&)context;
- (void)exitAcceleratedCompositingMode;
- (WebCore::IntRect)getFullScreenRect;
- (void)close;

@end

#endif
