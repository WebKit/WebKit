/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#import "WebInspectorClient.h"

#import "WebFrameInternal.h"
#import "WebFrameView.h"
#import "WebNodeHighlight.h"
#import "WebPreferences.h"
#import "WebView.h"
#import "WebViewInternal.h"
#import "WebViewPrivate.h"
#import <AppKit/NSWindowController.h>
#import <WebCore/InspectorController.h>
#import <WebCore/Page.h>
#import <WebKit/DOMCore.h>
#import <WebKit/DOMExtensions.h>

using namespace WebCore;

@interface WebInspectorWindowController : NSWindowController {
@private
    WebView *_inspectedWebView;
    WebView *_webView;
    NSImageView *_shadowView;
    WebNodeHighlight *_currentHighlight;
    BOOL _attachedToInspectedWebView;
    BOOL _shouldAttach;
    BOOL _visible;
    BOOL _movingWindows;
}
- (id)initWithInspectedWebView:(WebView *)webView;
- (BOOL)inspectorVisible;
- (WebView *)webView;
- (void)attach;
- (void)detach;
- (void)highlightAndScrollToNode:(DOMNode *)node;
- (void)highlightNode:(DOMNode *)node;
- (void)hideHighlight;
@end

#pragma mark -

WebInspectorClient::WebInspectorClient(WebView *webView)
: m_webView(webView)
{
}

void WebInspectorClient::inspectorDestroyed()
{
    [[m_windowController.get() webView] close];
    delete this;
}

Page* WebInspectorClient::createPage()
{
    if (!m_windowController)
        m_windowController.adoptNS([[WebInspectorWindowController alloc] initWithInspectedWebView:m_webView]);

    return core([m_windowController.get() webView]);
}

void WebInspectorClient::showWindow()
{
    updateWindowTitle();
    [m_windowController.get() showWindow:nil];
}

void WebInspectorClient::closeWindow()
{
    [m_windowController.get() close];
}

void WebInspectorClient::attachWindow()
{
    [m_windowController.get() attach];
}

void WebInspectorClient::detachWindow()
{
    [m_windowController.get() detach];
}

void WebInspectorClient::highlight(Node* node)
{
    [m_windowController.get() highlightAndScrollToNode:kit(node)];
}

void WebInspectorClient::hideHighlight()
{
    [m_windowController.get() hideHighlight];
}

void WebInspectorClient::inspectedURLChanged(const String& newURL)
{
    m_inspectedURL = newURL;
    updateWindowTitle();
}

void WebInspectorClient::updateWindowTitle() const
{
    [[m_windowController.get() window] setTitle:[NSString stringWithFormat:@"Web Inspector %C %@", 0x2014, (NSString *)m_inspectedURL]];
}

#pragma mark -

#define WebKitInspectorAttachedViewHeightKey @"WebKitInspectorAttachedViewHeight"

@implementation WebInspectorWindowController
- (id)init
{
    if (![super initWithWindow:nil])
        return nil;

    // Keep preferences separate from the rest of the client, making sure we are using expected preference values.
    // One reason this is good is that it keeps the inspector out of history via "private browsing".

    WebPreferences *preferences = [[WebPreferences alloc] init];
    [preferences setAutosaves:NO];
    [preferences setPrivateBrowsingEnabled:YES];
    [preferences setLoadsImagesAutomatically:YES];
    [preferences setJavaScriptEnabled:YES];
    [preferences setAllowsAnimatedImages:YES];
    [preferences setLoadsImagesAutomatically:YES];
    [preferences setPlugInsEnabled:NO];
    [preferences setJavaEnabled:NO];
    [preferences setUserStyleSheetEnabled:NO];
    [preferences setTabsToLinks:NO];
    [preferences setMinimumFontSize:0];
    [preferences setMinimumLogicalFontSize:9];

    _webView = [[WebView alloc] init];
    [_webView setPreferences:preferences];
    [_webView setDrawsBackground:NO];
    [_webView setProhibitsMainFrameScrolling:YES];

    [preferences release];

    // FIXME: The InspectorController should give us the URL to inspector.html
    NSString *path = [[NSBundle bundleWithIdentifier:@"com.apple.WebCore"] pathForResource:@"inspector" ofType:@"html" inDirectory:@"inspector"];
    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:[NSURL fileURLWithPath:path]];
    [[_webView mainFrame] loadRequest:request];
    [request release];

    [self setWindowFrameAutosaveName:@"Web Inspector 2"];
    return self;
}

-(id)initWithInspectedWebView:(WebView *)webView
{
    if (![self init])
        return nil;

    // Don't retain to avoid a circular reference
    _inspectedWebView = webView;
    return self;
}

- (void)dealloc
{
    [_shadowView release];
    [_webView release];
    [super dealloc];
}

#pragma mark -

- (BOOL)inspectorVisible
{
    return _visible;
}

- (WebView *)webView
{
    return _webView;
}

- (NSWindow *)window
{
    NSWindow *window = [super window];
    if (window)
        return window;

    window = [[NSWindow alloc] initWithContentRect:NSMakeRect(60.0, 200.0, 750.0, 650.0)
        styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask) backing:NSBackingStoreBuffered defer:YES];
    [window setDelegate:self];
    [window setMinSize:NSMakeSize(400.0, 400.0)];

    [self setWindow:window];
    [window release];

    return window;
}

#pragma mark -

- (BOOL)windowShouldClose:(id)sender
{
    _visible = NO;

    [_inspectedWebView page]->inspectorController()->setWindowVisible(false);

    [_currentHighlight detachHighlight];
    [_currentHighlight setDelegate:nil];
    [_currentHighlight release];
    _currentHighlight = nil;

    return YES;
}

- (void)close
{
    if (!_visible)
        return;

    _visible = NO;

    [_inspectedWebView page]->inspectorController()->setWindowVisible(false);

    if (!_movingWindows) {
        [_currentHighlight detachHighlight];
        [_currentHighlight setDelegate:nil];
        [_currentHighlight release];
        _currentHighlight = nil;
    }

    if (_attachedToInspectedWebView) {
        if ([_inspectedWebView _isClosed])
            return;

        WebFrameView *frameView = [[_inspectedWebView mainFrame] frameView];

        NSRect frameViewRect = [frameView frame];
        NSRect finalFrameViewRect = NSMakeRect(0, 0, NSWidth(frameViewRect), NSHeight([_inspectedWebView frame]));
        NSMutableDictionary *frameViewAnimationInfo = [[NSMutableDictionary alloc] init];
        [frameViewAnimationInfo setObject:frameView forKey:NSViewAnimationTargetKey];
        [frameViewAnimationInfo setObject:[NSValue valueWithRect:finalFrameViewRect] forKey:NSViewAnimationEndFrameKey];

        ASSERT(_shadowView);
        NSRect shadowFrame = [_shadowView frame];
        shadowFrame = NSMakeRect(0, NSMinY(frameViewRect) - NSHeight(shadowFrame), NSWidth(frameViewRect), NSHeight(shadowFrame));
        [_shadowView setFrame:shadowFrame];

        [_shadowView removeFromSuperview];
        [_inspectedWebView addSubview:_shadowView positioned:NSWindowAbove relativeTo:_webView];

        NSRect finalShadowRect = NSMakeRect(0, -NSHeight(shadowFrame), NSWidth(shadowFrame), NSHeight(shadowFrame));
        NSMutableDictionary *shadowAnimationInfo = [[NSMutableDictionary alloc] init];
        [shadowAnimationInfo setObject:_shadowView forKey:NSViewAnimationTargetKey];
        [shadowAnimationInfo setObject:[NSValue valueWithRect:finalShadowRect] forKey:NSViewAnimationEndFrameKey];

        NSArray *animationInfo = [[NSArray alloc] initWithObjects:frameViewAnimationInfo, shadowAnimationInfo, nil];
        [frameViewAnimationInfo release];
        [shadowAnimationInfo release];

        NSViewAnimation *slideAnimation = [[NSViewAnimation alloc] initWithViewAnimations:animationInfo]; // released in animationDidEnd
        [animationInfo release];

        [slideAnimation setAnimationBlockingMode:NSAnimationBlocking];
        [slideAnimation setDelegate:self];

        [[_inspectedWebView window] display]; // display once to make sure we start in a good state
        [slideAnimation startAnimation];
    } else {
        [super close];
    }
}

- (IBAction)showWindow:(id)sender
{
    if (_visible)
        return;

    _visible = YES;

    [_inspectedWebView page]->inspectorController()->setWindowVisible(true);

    if (_shouldAttach) {
        WebFrameView *frameView = [[_inspectedWebView mainFrame] frameView];

        NSRect frameViewRect = [frameView frame];
        float attachedHeight = [[NSUserDefaults standardUserDefaults] integerForKey:WebKitInspectorAttachedViewHeightKey];
        attachedHeight = MAX(300.0, MIN(attachedHeight, (NSHeight(frameViewRect) * 0.6)));

        [_webView removeFromSuperview];
        [_inspectedWebView addSubview:_webView positioned:NSWindowBelow relativeTo:(NSView*)frameView];
        [_webView setFrame:NSMakeRect(0, 0, NSWidth(frameViewRect), attachedHeight)];
        [_webView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable | NSViewMaxYMargin)];

        [frameView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable | NSViewMinYMargin)];

        NSRect finalFrameViewRect = NSMakeRect(0, attachedHeight, NSWidth(frameViewRect), NSHeight(frameViewRect) - attachedHeight);
        NSMutableDictionary *frameViewAnimationInfo = [[NSMutableDictionary alloc] init];
        [frameViewAnimationInfo setObject:frameView forKey:NSViewAnimationTargetKey];
        [frameViewAnimationInfo setObject:[NSValue valueWithRect:finalFrameViewRect] forKey:NSViewAnimationEndFrameKey];

        if (!_shadowView) {
            NSString *imagePath = [[NSBundle bundleWithIdentifier:@"com.apple.WebCore"] pathForResource:@"attachedShadow" ofType:@"png" inDirectory:@"inspector/Images"];
            NSImage *image = [[NSImage alloc] initWithContentsOfFile:imagePath];
            _shadowView = [[NSImageView alloc] initWithFrame:NSMakeRect(0, -[image size].height, NSWidth(frameViewRect), [image size].height)];
            [_shadowView setImage:image];
            [_shadowView setImageScaling:NSScaleToFit];
            [_shadowView setImageFrameStyle:NSImageFrameNone];
            [_shadowView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable | NSViewMaxYMargin)];
        }

        NSRect shadowFrame = [_shadowView frame];
        shadowFrame = NSMakeRect(0, -NSHeight(shadowFrame), NSWidth(frameViewRect), NSHeight(shadowFrame));
        [_shadowView setFrame:shadowFrame];

        [_shadowView removeFromSuperview];
        [_inspectedWebView addSubview:_shadowView positioned:NSWindowAbove relativeTo:_webView];

        NSRect finalShadowRect = NSMakeRect(0, attachedHeight - NSHeight(shadowFrame), NSWidth(shadowFrame), NSHeight(shadowFrame));
        NSMutableDictionary *shadowAnimationInfo = [[NSMutableDictionary alloc] init];
        [shadowAnimationInfo setObject:_shadowView forKey:NSViewAnimationTargetKey];
        [shadowAnimationInfo setObject:[NSValue valueWithRect:finalShadowRect] forKey:NSViewAnimationEndFrameKey];

        NSArray *animationInfo = [[NSArray alloc] initWithObjects:frameViewAnimationInfo, shadowAnimationInfo, nil];
        [frameViewAnimationInfo release];
        [shadowAnimationInfo release];

        NSViewAnimation *slideAnimation = [[NSViewAnimation alloc] initWithViewAnimations:animationInfo]; // released in animationDidEnd
        [animationInfo release];

        [slideAnimation setAnimationBlockingMode:NSAnimationBlocking];
        [slideAnimation setDelegate:self];

        _attachedToInspectedWebView = YES;

        [[_inspectedWebView window] display]; // display once to make sure we start in a good state
        [slideAnimation startAnimation];
    } else {
        _attachedToInspectedWebView = NO;

        NSView *contentView = [[self window] contentView];
        [_webView setFrame:[contentView frame]];
        [_webView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
        [_webView removeFromSuperview];
        [contentView addSubview:_webView];

        [super showWindow:nil];
    }
}

#pragma mark -

- (void)attach
{
    if (_attachedToInspectedWebView)
        return;

    _shouldAttach = YES;

    if (_visible) {
        _movingWindows = YES;
        [self close];
        _movingWindows = NO;
    }

    [self showWindow:nil];
}

- (void)detach
{
    if (!_attachedToInspectedWebView)
        return;

    _shouldAttach = NO;

    if (_visible) {
        _movingWindows = YES; // set back to NO in animationDidEnd
        [self close];
    } else {
        [self showWindow:nil];
    }
}

#pragma mark -

- (void)highlightAndScrollToNode:(DOMNode *)node
{
    NSRect bounds = [node boundingBox];
    if (!NSIsEmptyRect(bounds)) {
        // FIXME: this needs to use the frame the node coordinates are in
        NSRect visible = [[[[_inspectedWebView mainFrame] frameView] documentView] visibleRect];
        BOOL needsScroll = !NSContainsRect(visible, bounds) && !NSContainsRect(bounds, visible);

        // only scroll if the bounds isn't in the visible rect and dosen't contain the visible rect
        if (needsScroll) {
            // scroll to the parent element if we aren't focused on an element
            DOMElement *element;
            if ([node isKindOfClass:[DOMElement class]])
                element = (DOMElement *)node;
            else
                element = (DOMElement *)[node parentNode];
            [element scrollIntoViewIfNeeded:YES];

            // give time for the scroll to happen
            [self performSelector:@selector(highlightNode:) withObject:node afterDelay:0.25];
        } else
            [self highlightNode:node];
    }
}

- (void)highlightNode:(DOMNode *)node
{
    // The scrollview's content view stays around between page navigations, so target it
    NSView *view = [[[[[_inspectedWebView mainFrame] frameView] documentView] enclosingScrollView] contentView];
    if (![view window])
        return; // skip the highlight if we have no window (e.g. hidden tab)

    if (!_currentHighlight) {
        _currentHighlight = [[WebNodeHighlight alloc] initWithTargetView:view];
        [_currentHighlight setDelegate:self];
        [_currentHighlight attachHighlight];
    }

    [_currentHighlight show];

    [_currentHighlight setHighlightedNode:node];

    // FIXME: this is a hack until we hook up a didDraw and didScroll call in WebHTMLView
    [[_currentHighlight highlightView] setNeedsDisplay:YES];
}

- (void)hideHighlight
{
    if (!_currentHighlight)
        return;
    [_currentHighlight hide];
    [_currentHighlight setHighlightedNode:nil];
}

#pragma mark -

- (void)animationDidEnd:(NSAnimation*)animation
{
    [animation release];

    [_shadowView removeFromSuperview];

    if (_movingWindows) {
        _movingWindows = NO;
        [self showWindow:nil];
    }
}

@end
