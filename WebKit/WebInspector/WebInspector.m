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

#import "WebInspector.h"
#import "WebInspectorInternal.h"

#import "WebFrame.h"
#import "WebFrameInternal.h"
#import "WebInspectorPanel.h"
#import "WebNodeHighlight.h"
#import "WebPreferences.h"
#import "WebScriptDebugDelegate.h"
#import "WebTypesInternal.h"
#import "WebView.h"
#import "WebViewPrivate.h"

#import <WebKit/DOMCore.h>
#import <WebKit/DOMExtensions.h>

@interface NSWindow (NSWindowPrivate)
- (void)_setContentHasShadow:(BOOL)hasShadow;
@end

#pragma mark -

@implementation WebInspector

+ (WebInspector *)sharedWebInspector
{
    static WebInspector *_sharedWebInspector = nil;
    if (!_sharedWebInspector) {
        _sharedWebInspector = [[self alloc] init];
        _sharedWebInspector->_private->isSharedInspector = YES;
    }

    return _sharedWebInspector;
}

#pragma mark -

- (id)init
{
    if (![super initWithWindow:nil])
        return nil;

    [self setWindowFrameAutosaveName:@"Web Inspector"];

    _private = [[WebInspectorPrivate alloc] init];

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

    _private->webView = [[WebView alloc] init];
    [_private->webView setPreferences:preferences];
    [_private->webView setFrameLoadDelegate:self];
    [_private->webView setUIDelegate:self];
#ifndef NDEBUG
    [_private->webView setScriptDebugDelegate:self];
#endif
    [_private->webView setDrawsBackground:NO];
    [_private->webView setProhibitsMainFrameScrolling:YES];
    [_private->webView _setDashboardBehavior:WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows to:YES];
    [_private->webView _setDashboardBehavior:WebDashboardBehaviorAlwaysAcceptsFirstMouse to:YES];

    [preferences release];

    NSString *path = [[NSBundle bundleForClass:[self class]] pathForResource:@"inspector" ofType:@"html" inDirectory:@"webInspector"];
    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:[NSURL fileURLWithPath:path]];
    [[_private->webView mainFrame] loadRequest:request];
    [request release];

    while (!_private->webViewLoaded)
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]];

    return self;
}

- (id)initWithWebFrame:(WebFrame *)webFrame
{
    if (![self init])
        return nil;
    [self setWebFrame:webFrame];
    return self;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

#pragma mark -

- (NSWindow *)window
{
    NSWindow *window = [super window];
    if (!window) {
        NSPanel *window = [[WebInspectorPanel alloc] initWithContentRect:NSMakeRect(60.0f, 200.0f, 350.0f, 550.0f)
            styleMask:(NSBorderlessWindowMask | NSUtilityWindowMask) backing:NSBackingStoreBuffered defer:YES];
        [window setBackgroundColor:[NSColor clearColor]];
        [window setOpaque:NO];
        [window setHasShadow:YES];
        [window _setContentHasShadow:NO];
        [window setWorksWhenModal:YES];
        [window setAcceptsMouseMovedEvents:YES];
        [window setIgnoresMouseEvents:NO];
        [window setFloatingPanel:YES];
        [window setReleasedWhenClosed:YES];
        [window setMovableByWindowBackground:YES];
        [window setHidesOnDeactivate:NO];
        [window setDelegate:self];
        [window setMinSize:NSMakeSize(280.0f, 450.0f)];

        [_private->webView setFrame:[[window contentView] frame]];
        [window setContentView:_private->webView];

        [self setWindow:window];
        return window;
    }

    return window;
}

- (void)windowWillClose:(NSNotification *)notification
{
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSApplicationWillResignActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSApplicationDidBecomeActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:WebNodeHighlightExpiredNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSViewFrameDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSSystemColorsDidChangeNotification object:nil];

    [self setRootDOMNode:nil];
    [self setFocusedDOMNode:nil];
    [self setWebFrame:nil];

    if (!_private->isSharedInspector)
        [self release];
}

- (IBAction)showWindow:(id)sender
{
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_nodeHighlightExpired:) name:WebNodeHighlightExpiredNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_updateSystemColors) name:NSSystemColorsDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationWillResignActive) name:NSApplicationWillResignActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationDidBecomeActive) name:NSApplicationDidBecomeActiveNotification object:nil];

    [super showWindow:sender];
}

#pragma mark -

- (void)setWebFrame:(WebFrame *)webFrame
{
    if ([webFrame isEqual:_private->inspectedWebFrame])
        return;

    if (_private->inspectedWebFrame) {
        [[NSNotificationCenter defaultCenter] removeObserver:self name:WebViewProgressFinishedNotification object:[_private->inspectedWebFrame webView]];
        [_private->inspectedWebFrame _removeInspector:self];
    }

    id oldFrame = _private->inspectedWebFrame;
    _private->inspectedWebFrame = [webFrame retain];
    [oldFrame release];

    if (_private->inspectedWebFrame) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(inspectedWebViewProgressFinished:) name:WebViewProgressFinishedNotification object:[_private->inspectedWebFrame webView]];
        [_private->inspectedWebFrame _addInspector:self];
    }

    _private->preventHighlight = YES;
    [self setFocusedDOMNode:[webFrame DOMDocument]];
    _private->preventHighlight = NO;
}

- (WebFrame *)webFrame
{
    return _private->inspectedWebFrame;
}

#pragma mark -

- (void)setRootDOMNode:(DOMNode *)node
{
    NSArray *args = [[NSArray alloc] initWithObjects:(node ? (id)node : (id)[NSNull null]), nil];
    [[_private->webView windowScriptObject] callWebScriptMethod:@"updateRootNode" withArguments:args];
    [args release];
}

- (DOMNode *)rootDOMNode
{
    return [[_private->webView windowScriptObject] valueForKey:@"rootDOMNode"];
}

#pragma mark -

- (void)setFocusedDOMNode:(DOMNode *)node
{
    NSArray *args = [[NSArray alloc] initWithObjects:(node ? (id)node : (id)[NSNull null]), nil];
    [[_private->webView windowScriptObject] callWebScriptMethod:@"updateFocusedNode" withArguments:args];
    [args release];
}

- (DOMNode *)focusedDOMNode
{
    return [[_private->webView windowScriptObject] valueForKey:@"focusedDOMNode"];
}
@end

#pragma mark -

@implementation WebInspector (WebInspectorScripting)
- (void)showOptionsMenu
{
    NSMenu *menu = [[NSMenu alloc] init];
    [menu setAutoenablesItems:NO];

    NSMenuItem *item = [[NSMenuItem alloc] init];
    [item setTitle:@"Ignore Whitespace"];
    [item setTarget:self];
    [item setAction:@selector(_toggleIgnoreWhitespace:)];

    id value = [[_private->webView windowScriptObject] valueForKey:@"ignoreWhitespace"];
    [item setState:(value && [value isKindOfClass:[NSNumber class]] && [value boolValue] ? NSOnState : NSOffState)];

    [menu addItem:item];
    [item release];

    item = [[NSMenuItem alloc] init];
    [item setTitle:@"Show User Agent Styles"];
    [item setTarget:self];
    [item setAction:@selector(_toggleShowUserAgentStyles:)];

    value = [[_private->webView windowScriptObject] valueForKey:@"showUserAgentStyles"];
    [item setState:(value && [value isKindOfClass:[NSNumber class]] && [value boolValue] ? NSOnState : NSOffState)];

    [menu addItem:item];
    [item release];

    [NSMenu popUpContextMenu:menu withEvent:[[self window] currentEvent] forView:_private->webView];
    [menu release];

    // hack to force a layout and balance mouse events
    NSEvent *currentEvent = [[self window] currentEvent];
    NSEvent *event = [NSEvent mouseEventWithType:NSLeftMouseUp location:[currentEvent locationInWindow] modifierFlags:[currentEvent modifierFlags] timestamp:[currentEvent timestamp] windowNumber:[[currentEvent window] windowNumber] context:[currentEvent context] eventNumber:[currentEvent eventNumber] clickCount:[currentEvent clickCount] pressure:[currentEvent pressure]];
    [[[[_private->webView mainFrame] frameView] documentView] mouseUp:event];
}

- (void)highlightDOMNode:(DOMNode *)node
{
    if (!_private->preventHighlight) {
        NSRect bounds = [node boundingBox];
        if (!NSIsEmptyRect(bounds)) {
            NSRect visible = [[[_private->inspectedWebFrame frameView] documentView] visibleRect];
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
                [self performSelector:@selector(_highlightNode:) withObject:node afterDelay:0.25];
            } else
                [self _highlightNode:node];
        } else
            [_private->currentHighlight expire];
    }
}
@end

#pragma mark -

@implementation WebInspector (WebInspectorPrivate)
- (IBAction)_toggleIgnoreWhitespace:(id)sender
{
    [[_private->webView windowScriptObject] callWebScriptMethod:@"toggleIgnoreWhitespace" withArguments:nil];
}

- (IBAction)_toggleShowUserAgentStyles:(id)sender
{
    [[_private->webView windowScriptObject] callWebScriptMethod:@"toggleShowUserAgentStyles" withArguments:nil];
}

- (void)_highlightNode:(DOMNode *)node
{
    if (_private->currentHighlight) {
        [_private->currentHighlight expire];
        [_private->currentHighlight release];
        _private->currentHighlight = nil;
    }

    NSView *view = [[_private->inspectedWebFrame frameView] documentView];
    if (![view window])
        return; // skip the highlight if we have no window (e.g. hidden tab)

    NSRect bounds = NSIntersectionRect([node boundingBox], [view visibleRect]);
    if (!NSIsEmptyRect(bounds)) {
        NSArray *rects = nil;
        if ([node isKindOfClass:[DOMElement class]]) {
            DOMCSSStyleDeclaration *style = [[node ownerDocument] getComputedStyle:(DOMElement *)node pseudoElement:@""];
            if ([[style getPropertyValue:@"display"] isEqualToString:@"inline"])
                rects = [[node lineBoxRects] retain];
        } else if ([node isKindOfClass:[DOMText class]]
#if ENABLE(SVG)
                   && ![[node parentNode] isKindOfClass:NSClassFromString(@"DOMSVGElement")]
#endif
                  )
            rects = [[node lineBoxRects] retain];

        if (![rects count])
            rects = [[NSArray alloc] initWithObjects:[NSValue valueWithRect:bounds], nil];

        _private->currentHighlight = [[WebNodeHighlight alloc] initWithBounds:bounds andRects:rects forView:view];

        [rects release];
    }
}

- (void)_nodeHighlightExpired:(NSNotification *)notification
{
    if (_private->currentHighlight == [notification object]) {
        [_private->currentHighlight release];
        _private->currentHighlight = nil;
    }
}

- (void)_updateSystemColors
{
    CGFloat red = 0.0f, green = 0.0f, blue = 0.0f;
    NSColor *color = [[NSColor alternateSelectedControlColor] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    [color getRed:&red green:&green blue:&blue alpha:NULL];

    NSString *colorText = [NSString stringWithFormat:@"rgba(%.0f, %.0f, %.0f, 0.4) !important", (red * 255), (green * 255), (blue * 255)];
    NSString *styleText = [NSString stringWithFormat:@".focused .selected { background-color: %@ } .blured .selected { border-color: %@ }", colorText, colorText];
    DOMDocument *document = [[_private->webView mainFrame] DOMDocument];
    DOMElement *style = [document getElementById:@"systemColors"];
    if (!style) {
        style = [document createElement:@"style"];
        [style setAttribute:@"id" value:@"systemColors"];
        [[[document getElementsByTagName:@"head"] item:0] appendChild:style];
    }

    [style setTextContent:styleText];
}

- (void)_applicationWillResignActive
{
    [(NSPanel *)[self window] setFloatingPanel:NO];
}

- (void)_applicationDidBecomeActive
{
    [(NSPanel *)[self window] setFloatingPanel:YES];
}

- (void)_webFrameDetached:(WebFrame *)frame
{
    [self setRootDOMNode:nil];
    [self setFocusedDOMNode:nil];
    [self setWebFrame:nil];
}

#pragma mark -

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    return NO;
}

+ (NSString *)webScriptNameForSelector:(SEL)aSelector {
    NSMutableString *name = [[NSStringFromSelector(aSelector) mutableCopy] autorelease];
    [name replaceOccurrencesOfString:@":" withString:@"_" options:NSLiteralSearch range:NSMakeRange(0, [name length])];
    if ([name hasSuffix:@"_"])
        return [name substringToIndex:[name length] - 1];
    return name;
}

+ (BOOL)isKeyExcludedFromWebScript:(const char *)name
{
    return NO;
}

#pragma mark -

- (void)inspectedWebViewProgressFinished:(NSNotification *)notification
{
    if ([notification object] == [[self webFrame] webView])
        [self setFocusedDOMNode:[[self webFrame] DOMDocument]];
}

#pragma mark -

- (void)webView:(WebView *)sender windowScriptObjectAvailable:(WebScriptObject *)windowScriptObject
{
    // note: this is the Inspector's own WebView, not the one being inspected
    [[sender windowScriptObject] setValue:self forKey:@"Inspector"];
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    // note: this is the Inspector's own WebView, not the one being inspected
    _private->webViewLoaded = YES;

    [[self window] invalidateShadow];

    [self _updateSystemColors];
}

- (void)webView:(WebView *)sender runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    // note: this is the Inspector's own WebView, not the one being inspected
#ifndef NDEBUG
    NSLog(@"%@", message);
#endif
}

#ifndef NDEBUG
- (void)webView:(WebView *)view didParseSource:(NSString *)source baseLineNumber:(unsigned)baseLine fromURL:(NSURL *)url sourceId:(int)sid forWebFrame:(WebFrame *)webFrame
{
    // note: this is the Inspector's own WebView, not the one being inspected
    if (!_private->debugFileMap)
        _private->debugFileMap = [[NSMutableDictionary alloc] init];
    if (url)
        [_private->debugFileMap setObject:url forKey:[NSNumber numberWithInt:sid]];
}

- (void)webView:(WebView *)view exceptionWasRaised:(WebScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno forWebFrame:(WebFrame *)webFrame
{
    // note: this is the Inspector's own WebView, not the one being inspected
    NSURL *url = [_private->debugFileMap objectForKey:[NSNumber numberWithInt:sid]];
    NSLog(@"JS exception: %@ on %@ line %d", [[frame exception] valueForKey:@"message"], url, lineno);

    NSMutableArray *stack = [[NSMutableArray alloc] init];
    WebScriptCallFrame *currentFrame = frame;
    while (currentFrame) {
        if ([currentFrame functionName])
            [stack addObject:[currentFrame functionName]];
        else if ([frame caller])
            [stack addObject:@"(anonymous function)"];
        else
            [stack addObject:@"(global scope)"];
        currentFrame = [currentFrame caller];
    }

    NSLog(@"Stack trace:\n%@", [stack componentsJoinedByString:@"\n"]);
    [stack release];
}
#endif
@end

#pragma mark -

@implementation WebInspectorPrivate
- (void)dealloc
{
    [webView release];
    [inspectedWebFrame release];
    [currentHighlight release];
#ifndef NDEBUG
    [debugFileMap release];
#endif
    [super dealloc];
}
@end
