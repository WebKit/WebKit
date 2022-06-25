/*
 * Copyright (C) 2005, 2015 Apple Inc.  All rights reserved.
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

#import "WebClipView.h"

#if !PLATFORM(IOS_FAMILY)

#import "WebFrameInternal.h"
#import "WebFrameView.h"
#import "WebViewPrivate.h"
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>

// WebClipView's entire reason for existing is to set the clip used by focus ring redrawing.
// There's no easy way to prevent the focus ring from drawing outside the passed-in clip rectangle
// because it expects to have to draw outside the bounds of the view it's being drawn for.
// But it looks for the enclosing clip view, which gives us a hook we can use to control it.
// The "additional clip" is a clip for focus ring redrawing.

// FIXME: Change terminology from "additional clip" to "focus ring clip".

@interface NSView (WebViewMethod)
- (WebView *)_webView;
@end

@interface NSClipView (WebNSClipViewDetails)
- (void)_immediateScrollToPoint:(NSPoint)newOrigin;
- (BOOL)_canCopyOnScrollForDeltaX:(CGFloat)deltaX deltaY:(CGFloat)deltaY;
@end

@interface NSWindow (WebNSWindowDetails)
- (void)_disableDelayedWindowDisplay;
- (void)_enableDelayedWindowDisplay;
@end

@implementation WebClipView

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // In WebHTMLView, we set a clip. This is not typical to do in an
    // NSView, and while correct for any one invocation of drawRect:,
    // it causes some bad problems if that clip is cached between calls.
    // The cached graphics state, which clip views keep around, does
    // cache the clip in this undesirable way. Consequently, we want to 
    // release the GState for all clip views for all views contained in 
    // a WebHTMLView. Here we do it for subframes, which use WebClipView.
    // See these bugs for more information:
    // <rdar://problem/3409315>: REGRESSSION (7B58-7B60)?: Safari draws blank frames on macosx.apple.com perf page
    [self releaseGState];
    ALLOW_DEPRECATED_DECLARATIONS_END

    return self;
}

- (NSRect)visibleRect
{
    if (!_isScrolling)
        return [super visibleRect];

    WebFrameView *webFrameView = (WebFrameView *)[[self superview] superview];
    if (![webFrameView isKindOfClass:[WebFrameView class]])
        return [super visibleRect];

    if (auto* coreFrame = core([webFrameView webFrame])) {
        if (auto* frameView = coreFrame->view()) {
            if (frameView->isEnclosedInCompositingLayer())
                return [self bounds];
        }
    }

    return [super visibleRect];
}

- (void)_immediateScrollToPoint:(NSPoint)newOrigin
{
    _isScrolling = YES;
    _currentScrollIsBlit = NO;

    [[self window] _disableDelayedWindowDisplay];

    [super _immediateScrollToPoint:newOrigin];

    [[self window] _enableDelayedWindowDisplay];

    // We may hit this immediate scrolling code during a layout operation trigged by an AppKit call. When
    // this happens, WebCore will not paint. So, we need to mark this region dirty so that it paints properly.
    auto *webFrameView = (WebFrameView *)[[self superview] superview];
    if ([webFrameView isKindOfClass:[WebFrameView class]]) {
        if (auto* coreFrame = core([webFrameView webFrame])) {
            if (auto* frameView = coreFrame->view()) {
                if (!frameView->layoutContext().inPaintableState())
                    [self setNeedsDisplay:YES];
            }
        }
    }

    _isScrolling = NO;
}

- (BOOL)_canCopyOnScrollForDeltaX:(CGFloat)deltaX deltaY:(CGFloat)deltaY
{
    _currentScrollIsBlit = [super _canCopyOnScrollForDeltaX:deltaX deltaY:deltaY];
    return _currentScrollIsBlit;
}

- (BOOL)currentScrollIsBlit
{
    return _currentScrollIsBlit;
}

- (void)resetAdditionalClip
{
    ASSERT(_haveAdditionalClip);
    _haveAdditionalClip = NO;
}

- (void)setAdditionalClip:(NSRect)additionalClip
{
    ASSERT(!_haveAdditionalClip);
    _haveAdditionalClip = YES;
    _additionalClip = additionalClip;
}

- (BOOL)hasAdditionalClip
{
    return _haveAdditionalClip;
}

- (NSRect)additionalClip
{
    ASSERT(_haveAdditionalClip);
    return _additionalClip;
}

- (NSRect)_focusRingVisibleRect
{
    NSRect rect = [self visibleRect];
    if (_haveAdditionalClip)
        rect = NSIntersectionRect(rect, _additionalClip);
    return rect;
}

@end

#endif
