/*
 * Copyright (C) 2005 Apple Inc.  All rights reserved.
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

#import "WebDefaultUIDelegate.h"

#import "WebTypesInternal.h"
#import "WebUIDelegatePrivate.h"
#import "WebView.h"

#if !PLATFORM(IOS)
#import "WebJavaScriptTextInputPanel.h"
#endif

#if PLATFORM(IOS)
#import <WebCore/WAKViewPrivate.h>
#import <WebCore/WAKWindow.h>
#import <WebCore/WKViewPrivate.h>
#endif

#if !PLATFORM(IOS)
@interface NSApplication (DeclarationStolenFromAppKit)
- (void)_cycleWindowsReversed:(BOOL)reversed;
@end
#endif

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

- (WebView *)webView: (WebView *)wv createWebViewWithRequest:(NSURLRequest *)request windowFeatures:(NSDictionary *)features
{
    // If the new API method doesn't exist, fallback to the old version of createWebViewWithRequest
    // for backwards compatability
    if (![[wv UIDelegate] respondsToSelector:@selector(webView:createWebViewWithRequest:windowFeatures:)] && [[wv UIDelegate] respondsToSelector:@selector(webView:createWebViewWithRequest:)])
        return [[wv UIDelegate] webView:wv createWebViewWithRequest:request];
    return nil;
}

#if PLATFORM(IOS)
- (WebView *)webView:(WebView *)sender createWebViewWithRequest:(NSURLRequest *)request userGesture:(BOOL)userGesture
{
    return nil;
}
#endif

- (void)webViewShow: (WebView *)wv
{
}

- (void)webViewClose: (WebView *)wv
{
#if !PLATFORM(IOS)
    [[wv window] close];
#endif
}

- (void)webViewFocus: (WebView *)wv
{
#if !PLATFORM(IOS)
    [[wv window] makeKeyAndOrderFront:wv];
#endif
}

- (void)webViewUnfocus: (WebView *)wv
{
#if !PLATFORM(IOS)
    if ([[wv window] isKeyWindow] || [[[wv window] attachedSheet] isKeyWindow]) {
        [NSApp _cycleWindowsReversed:FALSE];
    }
#endif
}

- (NSResponder *)webViewFirstResponder: (WebView *)wv
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

- (void)webView: (WebView *)wv mouseDidMoveOverElement:(NSDictionary *)elementInformation modifierFlags:(NSUInteger)modifierFlags
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
#if PLATFORM(IOS)
    return NO;
#else
    return [[wv window] showsResizeIndicator];
#endif
}

- (void)webView: (WebView *)wv setResizable:(BOOL)resizable
{
#if !PLATFORM(IOS)
    // FIXME: This doesn't actually change the resizability of the window,
    // only visibility of the indicator.
    [[wv window] setShowsResizeIndicator:resizable];
#endif
}

- (void)webView: (WebView *)wv setFrame:(NSRect)frame
{
#if !PLATFORM(IOS)
    [[wv window] setFrame:frame display:YES];
#endif
}

- (NSRect)webViewFrame: (WebView *)wv
{
    NSWindow *window = [wv window];
    return window == nil ? NSZeroRect : [window frame];
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
#if !PLATFORM(IOS)
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
#else
    return nil;
#endif
}

- (void)webView: (WebView *)wv runOpenPanelForFileButtonWithResultListener:(id<WebOpenPanelResultListener>)resultListener
{
    // FIXME: We want a default here, but that would add localized strings.
}

- (void)webView:(WebView *)sender printFrameView:(WebFrameView *)frameView
{
}


#if !PLATFORM(IOS)
- (BOOL)webView:(WebView *)webView shouldBeginDragForElement:(NSDictionary *)element dragImage:(NSImage *)dragImage mouseDownEvent:(NSEvent *)mouseDownEvent mouseDraggedEvent:(NSEvent *)mouseDraggedEvent
{
    return YES;
}

- (NSUInteger)webView:(WebView *)webView dragDestinationActionMaskForDraggingInfo:(id <NSDraggingInfo>)draggingInfo
{
    return WebDragDestinationActionAny;
}

- (void)webView:(WebView *)webView willPerformDragDestinationAction:(WebDragDestinationAction)action forDraggingInfo:(id <NSDraggingInfo>)draggingInfo
{
}

- (NSUInteger)webView:(WebView *)webView dragSourceActionMaskForPoint:(NSPoint)point
{
    return WebDragSourceActionAny;
}

- (void)webView:(WebView *)webView willPerformDragSourceAction:(WebDragSourceAction)action fromPoint:(NSPoint)point withPasteboard:(NSPasteboard *)pasteboard
{
}
#endif

- (void)webView:(WebView *)sender didDrawRect:(NSRect)rect
{
}

- (void)webView:(WebView *)sender didScrollDocumentInFrameView:(WebFrameView *)frameView
{
}

#if !PLATFORM(IOS)
- (void)webView:(WebView *)sender willPopupMenu:(NSMenu *)menu
{
}

- (void)webView:(WebView *)sender contextMenuItemSelected:(NSMenuItem *)item forElement:(NSDictionary *)element
{
}
#endif

- (void)webView:(WebView *)sender exceededApplicationCacheOriginQuotaForSecurityOrigin:(WebSecurityOrigin *)origin totalSpaceNeeded:(NSUInteger)totalSpaceNeeded
{
}

- (BOOL)webView:(WebView *)sender shouldReplaceUploadFile:(NSString *)path usingGeneratedFilename:(NSString **)filename
{
    return NO;
}

- (NSString *)webView:(WebView *)sender generateReplacementFile:(NSString *)path
{
    return nil;
}

#if PLATFORM(IOS)
- (void)webViewSupportedOrientationsUpdated:(WebView *)sender
{
}
#endif

@end
