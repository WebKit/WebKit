/*
 * Copyright (C) 2005, 2006, 2007 Apple, Inc.  All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2007 Eric Seidel <eric@webkit.org>
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
 
#import "config.h"
#import "DumpRenderTreeWindow.h"

#import "DumpRenderTree.h"

// FIXME: This file is ObjC++ only because of this include. :(
#import "TestRunner.h"
#import <WebKit/WebViewPrivate.h>

#if PLATFORM(IOS_FAMILY)
#import <QuartzCore/CALayer.h>
#endif

CFMutableArrayRef openWindowsRef = 0;

static CFArrayCallBacks NonRetainingArrayCallbacks = {
    0,
    NULL,
    NULL,
    CFCopyDescription,
    CFEqual
};

@implementation DumpRenderTreeWindow

#if PLATFORM(IOS_FAMILY)
@synthesize uiWindow = _uiWindow;
@synthesize browserView = _browserView;
#endif

+ (NSArray *)openWindows
{
    return [[(NSArray *)openWindowsRef copy] autorelease];
}

- (void)_addToOpenWindows
{
    if (!openWindowsRef)
        openWindowsRef = CFArrayCreateMutable(NULL, 0, &NonRetainingArrayCallbacks);

    CFArrayAppendValue(openWindowsRef, (__bridge CFTypeRef)self);
}

#if !PLATFORM(IOS_FAMILY)
- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask backing:(NSBackingStoreType)bufferingType defer:(BOOL)deferCreation
{
    if ((self = [super initWithContentRect:contentRect styleMask:styleMask backing:bufferingType defer:deferCreation]))
        [self _addToOpenWindows];
    return self;
}
#else
- (id)initWithLayer:(CALayer *)layer
{
    if ((self = [super initWithLayer:layer])) {
        [self setEntireWindowVisibleForTesting:YES];
        [self _addToOpenWindows];
    }

    return self;
}

- (void)dealloc
{
    ASSERT(!_browserView);
    ASSERT(!_uiWindow);
    [super dealloc];
}
#endif

- (void)close
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    CFRange arrayRange = CFRangeMake(0, CFArrayGetCount(openWindowsRef));
    CFIndex i = CFArrayGetFirstIndexOfValue(openWindowsRef, arrayRange, (__bridge CFTypeRef)self);
    if (i != kCFNotFound)
        CFArrayRemoveValueAtIndex(openWindowsRef, i);

    [super close];

#if PLATFORM(IOS_FAMILY)
    // By default, NSWindows are released when closed. On iOS we do
    // it manually, and release the UIWindow and UIWebBrowserView.
    if (_uiWindow) {
        [_uiWindow release];
        _uiWindow = nil;
        [_browserView release];
        _browserView = nil;
        [self release];
    }
#endif
}

- (BOOL)isKeyWindow
{
    return gTestRunner ? gTestRunner->windowIsKey() : YES;
}

- (BOOL)_hasKeyAppearance
{
    return [self isKeyWindow];
}

#if !PLATFORM(IOS_FAMILY)
- (void)keyDown:(NSEvent *)event
{
    // Do nothing, avoiding the beep we'd otherwise get from NSResponder,
    // once we get to the end of the responder chain.
}
#endif

- (WebView *)webView
{
#if !PLATFORM(IOS_FAMILY)
    NSView *firstView = nil;
    if ([[[self contentView] subviews] count] > 0) {
        firstView = [[[self contentView] subviews] objectAtIndex:0];
        if ([firstView isKindOfClass:[WebView class]])
            return static_cast<WebView *>(firstView);
    }
    return nil;
#else
    ASSERT([[self contentView] isKindOfClass:[WebView class]]);
    return (WebView *)[self contentView];
#endif
}

- (void)startListeningForAcceleratedCompositingChanges
{
    [[self webView] _setPostsAcceleratedCompositingNotifications:YES];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(webViewStartedAcceleratedCompositing:)
        name:_WebViewDidStartAcceleratedCompositingNotification object:nil];
}

- (void)webViewStartedAcceleratedCompositing:(NSNotification *)notification
{
#if !PLATFORM(IOS_FAMILY)
    // If the WebView has gone into compositing mode, turn on window autodisplay. This is necessary for CA
    // to update layers and start animations.
    // We only ever turn autodisplay on here, because we turn it off before every test.
    if ([[self webView] _isUsingAcceleratedCompositing])
        [self setAutodisplay:YES];
#endif
}

@end
