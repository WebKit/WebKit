/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if PLATFORM(IOS_FAMILY)
#import <QuartzCore/CALayer.h>
#import <WebKitLegacy/WAKAppKitStubs.h>
#import <WebKitLegacy/WAKView.h>
#endif
#import <wtf/NakedPtr.h>

@class WebNodeHighlightView;
#if PLATFORM(IOS_FAMILY)
@class WebView;
#endif

namespace WebCore {
    class InspectorController;
}

#if PLATFORM(IOS_FAMILY)
@interface WebHighlightLayer : CALayer {
    WebNodeHighlightView *_view;
    WebView *_webView;
}
- (id)initWithHighlightView:(WebNodeHighlightView *)view webView:(WebView *)webView;
@end
#endif

@interface WebNodeHighlight : NSObject {
    NSView *_targetView;
#if !PLATFORM(IOS_FAMILY)
    NSWindow *_highlightWindow;
#else
    WebHighlightLayer *_highlightLayer;
#endif
    WebNodeHighlightView *_highlightView;
    NakedPtr<WebCore::InspectorController> _inspectorController;
    id _delegate;
}
- (id)initWithTargetView:(NSView *)targetView inspectorController:(NakedPtr<WebCore::InspectorController>)inspectorController;

- (void)setDelegate:(id)delegate;
- (id)delegate;

- (void)attach;
- (void)detach;

- (NSView *)targetView;
- (WebNodeHighlightView *)highlightView;

- (NakedPtr<WebCore::InspectorController>)inspectorController;

#if !PLATFORM(IOS_FAMILY)
- (void)setNeedsUpdateInTargetViewRect:(NSRect)rect;
#else
- (void)setNeedsDisplay;
#endif
@end

@interface NSObject (WebNodeHighlightDelegate)
- (void)didAttachWebNodeHighlight:(WebNodeHighlight *)highlight;
- (void)willDetachWebNodeHighlight:(WebNodeHighlight *)highlight;
@end
