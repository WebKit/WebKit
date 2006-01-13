/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import <Cocoa/Cocoa.h>

#import <Foundation/NSURLRequest.h>

#import <WebKit/WebDefaultUIDelegate.h>
#import <WebKit/WebJavaScriptTextInputPanel.h>
#import <WebKit/WebView.h>
#import <WebKit/WebUIDelegatePrivate.h>
#import <WebKit/DOM.h>

@interface NSApplication (DeclarationStolenFromAppKit)
- (void)_cycleWindowsReversed:(BOOL)reversed;
@end

@implementation WebDefaultUIDelegate

static WebDefaultUIDelegate *sharedDelegate = nil;

// Return a object with vanilla implementations of the protocol's methods
// Note this feature relies on our default delegate being stateless.  This
// is probably an invalid assumption for the WebUIDelegate.
// If we add any real functionality to this default delegate we probably
// won't be able to use a singleton.
+ (WebDefaultUIDelegate *)sharedUIDelegate
{
    if (!sharedDelegate) {
        sharedDelegate = [[WebDefaultUIDelegate alloc] init];
    }
    return sharedDelegate;
}

- (WebView *)webView: (WebView *)wv createWebViewWithRequest:(NSURLRequest *)request
{
    return nil;
}

- (void)webViewShow: (WebView *)wv
{
}

- (void)webViewClose: (WebView *)wv
{
    [[wv window] close];
}

- (void)webViewFocus: (WebView *)wv
{
    [[wv window] makeKeyAndOrderFront:wv];
}

- (void)webViewUnfocus: (WebView *)wv
{
    if ([[wv window] isKeyWindow] || [[[wv window] attachedSheet] isKeyWindow]) {
        [NSApp _cycleWindowsReversed:FALSE];
    }
}

- (NSResponder *)webViewFirstResponder: (WebView *)wv;
{
    return [[wv window] firstResponder];
}

- (void)webView: (WebView *)wv makeFirstResponder:(NSResponder *)responder
{
    [[wv window] makeFirstResponder:responder];
}

- (void)webView: (WebView *)wv setStatusText:(NSString *)text
{
}

- (NSString *)webViewStatusText: (WebView *)wv
{
    return nil;
}

- (void)webView: (WebView *)wv mouseDidMoveOverElement:(NSDictionary *)elementInformation modifierFlags:(unsigned int)modifierFlags
{
}

- (BOOL)webViewAreToolbarsVisible: (WebView *)wv
{
    return NO;
}

- (void)webView: (WebView *)wv setToolbarsVisible:(BOOL)visible
{
}

- (BOOL)webViewIsStatusBarVisible: (WebView *)wv
{
    return NO;
}

- (void)webView: (WebView *)wv setStatusBarVisible:(BOOL)visible
{
}

- (BOOL)webViewIsResizable: (WebView *)wv
{
    return [[wv window] showsResizeIndicator];
}

- (void)webView: (WebView *)wv setResizable:(BOOL)resizable;
{
    // FIXME: This doesn't actually change the resizability of the window,
    // only visibility of the indicator.
    [[wv window] setShowsResizeIndicator:resizable];
}

- (void)webView: (WebView *)wv setFrame:(NSRect)frame
{
    [[wv window] setFrame:frame display:YES];
}

- (NSRect)webViewFrame: (WebView *)wv
{
    NSWindow *window = [wv window];
    return window == nil ? NSZeroRect : [window frame];
}

- (void)webView:(WebView *)webView setContentRect:(NSRect)contentRect
{
    [self webView:webView setFrame:[NSWindow frameRectForContentRect:contentRect styleMask:[[webView window] styleMask]]];
}

- (NSRect)webViewContentRect:(WebView *)webView
{
    NSWindow *window = [webView window];
    return window == nil ? NSZeroRect : [NSWindow contentRectForFrameRect:[window frame] styleMask:[window styleMask]];
}

- (void)webView: (WebView *)wv runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    // FIXME: We want a default here, but that would add localized strings.
}

- (BOOL)webView: (WebView *)wv runJavaScriptConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    // FIXME: We want a default here, but that would add localized strings.
    return NO;
}

- (NSString *)webView: (WebView *)wv runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText initiatedByFrame:(WebFrame *)frame
{
    WebJavaScriptTextInputPanel *panel = [[WebJavaScriptTextInputPanel alloc] initWithPrompt:prompt text:defaultText];
    [panel showWindow:nil];
    NSString *result;
    if ([NSApp runModalForWindow:[panel window]]) {
        result = [panel text];
    } else {
        result = nil;
    }
    [[panel window] close];
    [panel release];
    return result;
}

- (void)webView: (WebView *)wv runOpenPanelForFileButtonWithResultListener:(id<WebOpenPanelResultListener>)resultListener
{
    // FIXME: We want a default here, but that would add localized strings.
}

- (void)webViewPrint:(WebView *)sender
{
}

- (void)webView:(WebView *)sender printFrameView:(WebFrameView *)frameView
{
}


- (BOOL)webView:(WebView *)webView shouldBeginDragForElement:(NSDictionary *)element dragImage:(NSImage *)dragImage mouseDownEvent:(NSEvent *)mouseDownEvent mouseDraggedEvent:(NSEvent *)mouseDraggedEvent
{
    return YES;
}

- (unsigned)webView:(WebView *)webView dragDestinationActionMaskForDraggingInfo:(id <NSDraggingInfo>)draggingInfo;
{
    return WebDragDestinationActionAny;
}

- (void)webView:(WebView *)webView willPerformDragDestinationAction:(WebDragDestinationAction)action forDraggingInfo:(id <NSDraggingInfo>)draggingInfo
{
}

- (unsigned)webView:(WebView *)webView dragSourceActionMaskForPoint:(NSPoint)point;
{
    DOMElement *elementAtPoint = [[webView elementAtPoint:point] objectForKey:WebElementDOMNodeKey];
    if ([elementAtPoint respondsToSelector:@selector(isContentEditable)] && [(id)elementAtPoint isContentEditable])
        return (WebDragSourceActionAny & ~WebDragSourceActionLink);

    return WebDragSourceActionAny;
}

- (void)webView:(WebView *)webView willPerformDragSourceAction:(WebDragSourceAction)action fromPoint:(NSPoint)point withPasteboard:(NSPasteboard *)pasteboard;
{
}

@end
