/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "Helpers/cocoa/FindStateSnapshot.h"

#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <wtf/Function.h>

namespace TestWebKitAPI {

static void captureFrameFindStates(TestWKWebView *webView, _WKFrameTreeNode *node, Vector<unsigned>& path, Vector<FrameFindState>& frames)
{
    NSDictionary *state = [webView objectByEvaluatingJavaScript:@"({ selectedText: getSelection().toString(), scrollX, scrollY, hasFocus: document.hasFocus() })" inFrame:node.info];
    frames.append({
        .path = path,
        .selectedText = state[@"selectedText"],
        .scrollPosition = CGPointMake([state[@"scrollX"] doubleValue], [state[@"scrollY"] doubleValue]),
        .hasFocus = static_cast<bool>([state[@"hasFocus"] boolValue]),
    });

    for (NSUInteger i = 0; i < node.childFrames.count; ++i) {
        path.append(i);
        captureFrameFindStates(webView, node.childFrames[i], path, frames);
        path.removeLast();
    }
}

static CGRect installedTextIndicatorRect(TestWKWebView *webView)
{
    CGRect result = CGRectNull;
    Function<void(CALayer *)> visit = [&](CALayer *layer) {
        if ([layer isKindOfClass:NSClassFromString(@"WebTextIndicatorLayer")])
            result = layer.frame;
        for (CALayer *sublayer in layer.sublayers)
            visit(sublayer);
    };
    visit(webView.layer);
    return result;
}

FindStateSnapshot FindStateSnapshot::capture(TestWKWebView *webView)
{
    FindStateSnapshot snapshot;
    Vector<unsigned> path;
    captureFrameFindStates(webView, [webView mainFrame], path, snapshot.frames);
    snapshot.textIndicatorRect = installedTextIndicatorRect(webView);
#if HAVE(UIFINDINTERACTION)
    snapshot.hasFindOverlayLayer = static_cast<bool>(webView._layerForFindOverlay);
#endif
#if PLATFORM(IOS_FAMILY)
    snapshot.contentOffset = webView.scrollView.contentOffset;
#endif
    return snapshot;
}

const FrameFindState& FindStateSnapshot::frame(const Vector<unsigned>& path) const
{
    for (auto& frame : frames) {
        if (frame.path == path)
            return frame;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

Vector<Vector<unsigned>> FindStateSnapshot::framesWithSelection() const
{
    Vector<Vector<unsigned>> result;
    for (auto& frame : frames) {
        if (!frame.selectedText.isEmpty())
            result.append(frame.path);
    }
    return result;
}

} // namespace TestWebKitAPI
