/*	
    WebDefaultPolicyDelegate.m
    Copyright 2003, Apple Computer, Inc.
*/
#import <Cocoa/Cocoa.h>

#import <WebFoundation/NSURLRequest.h>

#import <WebKit/WebDefaultUIDelegate.h>
#import <WebKit/WebJavaScriptTextInputPanel.h>
#import <WebKit/WebView.h>
#import <WebKit/WebUIDelegate.h>

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

- (void)dealloc
{
    [element release];
    [super dealloc];
}

- (WebView *)webView: (WebView *)wv createWindowWithRequest:(NSURLRequest *)request
{
    return nil;
}

- (void)webViewShowWindow: (WebView *)wv
{
}

- (void)webViewCloseWindow: (WebView *)wv
{
    [[wv window] close];
}

- (void)webViewFocusWindow: (WebView *)wv
{
    [[wv window] makeKeyAndOrderFront:wv];
}

- (void)webViewUnfocusWindow: (WebView *)wv
{
    if ([[wv window] isKeyWindow] || [[[wv window] attachedSheet] isKeyWindow]) {
	[NSApp _cycleWindowsReversed:FALSE];
    }
}

- (NSResponder *)webViewFirstResponderInWindow: (WebView *)wv;
{
    return [[wv window] firstResponder];
}

- (void)webView: (WebView *)wv makeFirstResponderInWindow:(NSResponder *)responder
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

- (void)webView: (WebView *)wv runJavaScriptAlertPanelWithMessage:(NSString *)message
{
    // FIXME: We want a default here, but that would add localized strings.
}

- (BOOL)webView: (WebView *)wv runJavaScriptConfirmPanelWithMessage:(NSString *)message
{
    // FIXME: We want a default here, but that would add localized strings.
    return NO;
}

- (NSString *)webView: (WebView *)wv runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText
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

@end
