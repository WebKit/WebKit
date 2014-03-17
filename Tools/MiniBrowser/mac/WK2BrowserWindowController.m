/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#import "WK2BrowserWindowController.h"

#if WK_API_ENABLED

#import "AppDelegate.h"
#import <WebKit2/WKFrameInfo.h>
#import <WebKit2/WKNavigationDelegate.h>
#import <WebKit2/WKUIDelegate.h>
#import <WebKit2/WKWebView.h>
#import <WebKit2/WKWebViewPrivate.h>

static void* keyValueObservingContext = &keyValueObservingContext;
static NSString * const WebKit2UseRemoteLayerTreeDrawingAreaKey = @"WebKit2UseRemoteLayerTreeDrawingArea";

@interface WK2BrowserWindowController () <WKNavigationDelegate, WKUIDelegate>
@end

@implementation WK2BrowserWindowController {
    WKWebView *_webView;
}

- (void)awakeFromNib
{
    _webView = [[WKWebView alloc] initWithFrame:[containerView bounds]];

    _webView.allowsMagnification = YES;
    _webView.allowsBackForwardNavigationGestures = YES;

    [_webView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    [containerView addSubview:_webView];

    [progressIndicator bind:NSHiddenBinding toObject:_webView withKeyPath:@"loading" options:@{ NSValueTransformerNameBindingOption : NSNegateBooleanTransformerName }];
    [progressIndicator bind:NSValueBinding toObject:_webView withKeyPath:@"estimatedProgress" options:nil];

    [_webView addObserver:self forKeyPath:@"title" options:0 context:keyValueObservingContext];
    [_webView addObserver:self forKeyPath:@"activeURL" options:0 context:keyValueObservingContext];

    _webView.navigationDelegate = self;
    _webView.UIDelegate = self;
}

- (void)dealloc
{
    [_webView removeObserver:self forKeyPath:@"title"];
    [_webView removeObserver:self forKeyPath:@"activeURL"];
    
    [progressIndicator unbind:NSHiddenBinding];
    [progressIndicator unbind:NSValueBinding];

    [_webView release];

    [super dealloc];
}

- (IBAction)fetch:(id)sender
{
    [urlText setStringValue:[self addProtocolIfNecessary:[urlText stringValue]]];

    [_webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[urlText stringValue]]]];
}

- (IBAction)showHideWebView:(id)sender
{
    BOOL hidden = ![_webView isHidden];
    
    [_webView setHidden:hidden];
}

- (IBAction)removeReinsertWebView:(id)sender
{
    if ([_webView window]) {
        [_webView retain];
        [_webView removeFromSuperview]; 
    } else {
        [containerView addSubview:_webView];
        [_webView release];
    }
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    SEL action = [menuItem action];

    if (action == @selector(zoomIn:))
        return [self canZoomIn];
    if (action == @selector(zoomOut:))
        return [self canZoomOut];
    if (action == @selector(resetZoom:))
        return [self canResetZoom];
    
    // Disabled until missing WK2 functionality is exposed via API/SPI.
    if (action == @selector(reload:)
        || action == @selector(toggleZoomMode:)
        || action == @selector(resetZoom:)
        || action == @selector(dumpSourceToConsole:)
        || action == @selector(find:)
        || action == @selector(togglePaginationMode:))
        return NO;
    
    if (action == @selector(showHideWebView:))
        [menuItem setTitle:[_webView isHidden] ? @"Show Web View" : @"Hide Web View"];
    else if (action == @selector(removeReinsertWebView:))
        [menuItem setTitle:[_webView window] ? @"Remove Web View" : @"Insert Web View"];
    else if ([menuItem action] == @selector(toggleTransparentWindow:))
        [menuItem setState:[[self window] isOpaque] ? NSOffState : NSOnState];
    else if ([menuItem action] == @selector(toggleUISideCompositing:))
        [menuItem setState:[self isUISideCompositingEnabled] ? NSOnState : NSOffState];

    return YES;
}

- (IBAction)reload:(id)sender
{
}

- (IBAction)forceRepaint:(id)sender
{
    [_webView setNeedsDisplay:YES];
}

- (IBAction)goBack:(id)sender
{
    [_webView goBack];
}

- (IBAction)goForward:(id)sender
{
    [_webView goForward];
}

- (IBAction)toggleZoomMode:(id)sender
{
}

- (IBAction)resetZoom:(id)sender
{
}

- (BOOL)canResetZoom
{
    return NO;
}

- (IBAction)dumpSourceToConsole:(id)sender
{
}

- (IBAction)togglePaginationMode:(id)sender
{
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    SEL action = [item action];

    if (action == @selector(goBack:))
        return _webView && [_webView canGoBack];
    
    if (action == @selector(goForward:))
        return _webView && [_webView canGoForward];
    
    return YES;
}

- (void)validateToolbar
{
    [toolbar validateVisibleItems];
}

- (BOOL)windowShouldClose:(id)sender
{
    return YES;
}

- (void)windowWillClose:(NSNotification *)notification
{
    [(BrowserAppDelegate *)[NSApp delegate] browserWindowWillClose:[self window]];
    [self autorelease];
}

- (void)applicationTerminating
{
}

#define DefaultMinimumZoomFactor (.5)
#define DefaultMaximumZoomFactor (3.0)
#define DefaultZoomFactorRatio (1.2)

- (CGFloat)currentZoomFactor
{
    return 1;
}

- (void)setCurrentZoomFactor:(CGFloat)factor
{
}

- (BOOL)canZoomIn
{
    return NO;
}

- (void)zoomIn:(id)sender
{
    if (![self canZoomIn])
        return;

    CGFloat factor = [self currentZoomFactor] * DefaultZoomFactorRatio;
    [self setCurrentZoomFactor:factor];
}

- (BOOL)canZoomOut
{
    return NO;
}

- (void)zoomOut:(id)sender
{
    if (![self canZoomIn])
        return;

    CGFloat factor = [self currentZoomFactor] / DefaultZoomFactorRatio;
    [self setCurrentZoomFactor:factor];
}

- (IBAction)toggleTransparentWindow:(id)sender
{
    BOOL isTransparent = _webView._drawsTransparentBackground;
    isTransparent = !isTransparent;

    [[self window] setOpaque:!isTransparent];
    [[self window] setHasShadow:!isTransparent];

    _webView._drawsTransparentBackground = isTransparent;

    [[self window] display];    
}

- (BOOL)isUISideCompositingEnabled
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:WebKit2UseRemoteLayerTreeDrawingAreaKey];
}

- (IBAction)toggleUISideCompositing:(id)sender
{
    NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
    BOOL newValue = ![userDefaults boolForKey:WebKit2UseRemoteLayerTreeDrawingAreaKey];
    [userDefaults setBool:newValue forKey:WebKit2UseRemoteLayerTreeDrawingAreaKey];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (context != keyValueObservingContext || object != _webView)
        return;

    if ([keyPath isEqualToString:@"title"])
        self.window.title = [_webView.title stringByAppendingString:@" [WK2]"];
    else if ([keyPath isEqualToString:@"activeURL"])
        [self updateTextFieldFromURL:_webView.activeURL];
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)())completionHandler
{
    NSAlert* alert = [[NSAlert alloc] init];

    [alert setMessageText:[NSString stringWithFormat:@"JavaScript alert dialog from %@.", [frame.request.URL absoluteString]]];
    [alert setInformativeText:message];
    [alert addButtonWithTitle:@"OK"];

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    [alert beginSheetModalForWindow:self.window completionHandler:^void (NSModalResponse response) {
        completionHandler();
        [alert release];
    }];
#endif
}

- (void)webView:(WKWebView *)webView runJavaScriptConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL result))completionHandler
{
    NSAlert* alert = [[NSAlert alloc] init];

    [alert setMessageText:[NSString stringWithFormat:@"JavaScript confirm dialog from %@.", [frame.request.URL  absoluteString]]];
    [alert setInformativeText:message];
    
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    [alert beginSheetModalForWindow:self.window completionHandler:^void (NSModalResponse response) {
        completionHandler(response == NSAlertFirstButtonReturn);
        [alert release];
    }];
#endif
}

- (void)webView:(WKWebView *)webView runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSString *result))completionHandler
{
    NSAlert* alert = [[NSAlert alloc] init];

    [alert setMessageText:[NSString stringWithFormat:@"JavaScript prompt dialog from %@.", [frame.request.URL absoluteString]]];
    [alert setInformativeText:prompt];
    
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];
    
    NSTextField* input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 200, 24)];
    [input setStringValue:defaultText];
    [alert setAccessoryView:input];
    
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    [alert beginSheetModalForWindow:self.window completionHandler:^void (NSModalResponse response) {
        [input validateEditing];
        completionHandler(response == NSAlertFirstButtonReturn ? [input stringValue] : nil);
        [alert release];
    }];
#endif
}

- (void)updateTextFieldFromURL:(NSURL *)URL
{
    if (!URL)
        return;

    if (!URL.absoluteString.length)
        return;

    urlText.stringValue = [URL absoluteString];
}

- (void)loadURLString:(NSString *)urlString
{
    // FIXME: We shouldn't have to set the url text here.
    [urlText setStringValue:urlString];
    [self fetch:nil];
}

- (IBAction)performFindPanelAction:(id)sender
{
    [findPanelWindow makeKeyAndOrderFront:sender];
}

- (IBAction)find:(id)sender
{
}

#pragma mark WKNavigationDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicyDecision))decisionHandler
{
    LOG(@"decidePolicyForNavigationResponse");
    decisionHandler(WKNavigationResponsePolicyDecisionAllow);
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    LOG(@"didStartProvisionalNavigation");
}

- (void)webView:(WKWebView *)webView didReceiveServerRedirectForProvisionalNavigation:(WKNavigation *)navigation
{
    LOG(@"didReceiveServerRedirectForProvisionalNavigation");
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    LOG(@"didFailProvisionalNavigation: %@", error);
}

- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation
{
    LOG(@"didCommitNavigation: %@", error);
}

- (void)webView:(WKWebView *)webView didFinishLoadingNavigation:(WKNavigation *)navigation
{
    LOG(@"didFinishLoadingNavigation");
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    LOG(@"didFailNavigation: %@", error);
}

@end

#endif // WK_API_ENABLED
