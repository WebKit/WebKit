//
//  WebClipView.m
//  WebKit
//
//  Created by Darin Adler on Tue Sep 24 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebClipView.h"

#import <WebKit/WebAssertions.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebViewPrivate.h>

// WebClipView's entire reason for existing is to set the clip used by focus ring redrawing.
// There's no easy way to prevent the focus ring from drawing outside the passed-in clip rectangle
// because it expects to have to draw outside the bounds of the view it's being drawn for.
// But it looks for the enclosing clip view, which gives us a hook we can use to control it.
// The "additional clip" is a clip for focus ring redrawing.

// FIXME: Change terminology from "additional clip" to "focus ring clip".

@implementation WebClipView

- (id)initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];
    
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
    
    return self;
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
    if (_haveAdditionalClip) {
        rect = NSIntersectionRect(rect, _additionalClip);
    }
    return rect;
}

- (void)scrollWheel:(NSEvent *)event
{
    NSView *docView = [self documentView];
    if ([docView respondsToSelector:@selector(_webView)]) {
	WebView *wv = [docView _webView];
	if ([wv _dashboardBehavior:WebDashboardBehaviorAllowWheelScrolling]) {
	    [super scrollWheel:event];
	}
	return;
    }
    [super scrollWheel:event];
}

@end
