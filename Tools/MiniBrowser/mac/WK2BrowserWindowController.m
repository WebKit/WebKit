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
#import <WebKit2/WKBrowsingContextControllerPrivate.h>
#import <WebKit2/WKBrowsingContextHistoryDelegate.h>
#import <WebKit2/WKBrowsingContextLoadDelegatePrivate.h>
#import <WebKit2/WKBrowsingContextPolicyDelegate.h>
#import <WebKit2/WKNavigationData.h>
#import <WebKit2/WKStringCF.h>
#import <WebKit2/WKURLCF.h>
#import <WebKit2/WKViewPrivate.h>

static void* keyValueObservingContext = &keyValueObservingContext;
static NSString * const WebKit2UseRemoteLayerTreeDrawingAreaKey = @"WebKit2UseRemoteLayerTreeDrawingArea";

@interface WK2BrowserWindowController () <WKBrowsingContextLoadDelegatePrivate, WKBrowsingContextPolicyDelegate, WKBrowsingContextHistoryDelegate>
@end

@implementation WK2BrowserWindowController {
    WKProcessGroup *_processGroup;
    WKBrowsingContextGroup *_browsingContextGroup;

    WKView *_webView;
}

- (id)initWithProcessGroup:(WKProcessGroup *)processGroup browsingContextGroup:(WKBrowsingContextGroup *)browsingContextGroup
{
    if ((self = [super initWithWindowNibName:@"BrowserWindow"])) {
        _processGroup = [processGroup retain];
        _browsingContextGroup = [browsingContextGroup retain];
        _zoomTextOnly = NO;
    }
    
    return self;
}

- (void)dealloc
{
    [progressIndicator unbind:NSHiddenBinding];
    [progressIndicator unbind:NSValueBinding];

    [_webView.browsingContextController removeObserver:self forKeyPath:@"title" context:keyValueObservingContext];
    [_webView.browsingContextController removeObserver:self forKeyPath:@"activeURL" context:keyValueObservingContext];
    _webView.browsingContextController.loadDelegate = nil;
    _webView.browsingContextController.policyDelegate = nil;
    [_webView release];

    [_browsingContextGroup release];
    [_processGroup release];

    [super dealloc];
}

- (IBAction)fetch:(id)sender
{
    [urlText setStringValue:[self addProtocolIfNecessary:[urlText stringValue]]];

    [_webView.browsingContextController loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[urlText stringValue]]]];
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

    if (action == @selector(showHideWebView:))
        [menuItem setTitle:[_webView isHidden] ? @"Show Web View" : @"Hide Web View"];
    else if (action == @selector(removeReinsertWebView:))
        [menuItem setTitle:[_webView window] ? @"Remove Web View" : @"Insert Web View"];
    else if (action == @selector(toggleZoomMode:))
        [menuItem setState:_zoomTextOnly ? NSOnState : NSOffState];
    else if ([menuItem action] == @selector(togglePaginationMode:))
        [menuItem setState:[self isPaginated] ? NSOnState : NSOffState];
    else if ([menuItem action] == @selector(toggleTransparentWindow:))
        [menuItem setState:[[self window] isOpaque] ? NSOffState : NSOnState];
    else if ([menuItem action] == @selector(toggleUISideCompositing:))
        [menuItem setState:[self isUISideCompositingEnabled] ? NSOnState : NSOffState];

    return YES;
}

- (IBAction)reload:(id)sender
{
    [_webView.browsingContextController reload];
}

- (IBAction)forceRepaint:(id)sender
{
    [_webView setNeedsDisplay:YES];
}

- (IBAction)goBack:(id)sender
{
    [_webView.browsingContextController goBack];
}

- (IBAction)goForward:(id)sender
{
    [_webView.browsingContextController goForward];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    SEL action = [item action];

    if (action == @selector(goBack:))
        return _webView && [_webView.browsingContextController canGoBack];
    
    if (action == @selector(goForward:))
        return _webView && [_webView.browsingContextController canGoForward];
    
    return YES;
}

- (void)validateToolbar
{
    [toolbar validateVisibleItems];
}

- (BOOL)windowShouldClose:(id)sender
{
    LOG(@"windowShouldClose");
    BOOL canCloseImmediately = WKPageTryClose(_webView.pageRef);
    return canCloseImmediately;
}

- (void)windowWillClose:(NSNotification *)notification
{
    [(BrowserAppDelegate *)[NSApp delegate] browserWindowWillClose:[self window]];
    [self autorelease];
}

- (void)applicationTerminating
{
    // FIXME: Why are we bothering to close the page? This doesn't even prevent LEAK output.
    WKPageClose(_webView.pageRef);
}

#define DefaultMinimumZoomFactor (.5)
#define DefaultMaximumZoomFactor (3.0)
#define DefaultZoomFactorRatio (1.2)

- (CGFloat)currentZoomFactor
{
    return _zoomTextOnly ? _webView.browsingContextController.textZoom : _webView.browsingContextController.pageZoom;
}

- (void)setCurrentZoomFactor:(CGFloat)factor
{
    if (_zoomTextOnly)
        _webView.browsingContextController.textZoom = factor;
    else
        _webView.browsingContextController.pageZoom = factor;
}

- (BOOL)canZoomIn
{
    return [self currentZoomFactor] * DefaultZoomFactorRatio < DefaultMaximumZoomFactor;
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
    return [self currentZoomFactor] / DefaultZoomFactorRatio > DefaultMinimumZoomFactor;
}

- (void)zoomOut:(id)sender
{
    if (![self canZoomIn])
        return;

    CGFloat factor = [self currentZoomFactor] / DefaultZoomFactorRatio;
    [self setCurrentZoomFactor:factor];
}

- (BOOL)canResetZoom
{
    return _zoomTextOnly ? (_webView.browsingContextController.textZoom != 1) : (_webView.browsingContextController.pageZoom != 1);
}

- (void)resetZoom:(id)sender
{
    if (![self canResetZoom])
        return;

    if (_zoomTextOnly)
        _webView.browsingContextController.textZoom = 1;
    else
        _webView.browsingContextController.pageZoom = 1;
}

- (IBAction)toggleZoomMode:(id)sender
{
    if (_zoomTextOnly) {
        _zoomTextOnly = NO;
        double currentTextZoom = _webView.browsingContextController.textZoom;
        WKPageSetPageAndTextZoomFactors(_webView.pageRef, currentTextZoom, 1);
    } else {
        _zoomTextOnly = YES;
        double currentPageZoom = _webView.browsingContextController.pageZoom;
        WKPageSetPageAndTextZoomFactors(_webView.pageRef, 1, currentPageZoom);
    }
}

- (BOOL)isPaginated
{
    return _webView.browsingContextController.paginationMode != WKPaginationModeUnpaginated;
}

- (IBAction)togglePaginationMode:(id)sender
{
    if ([self isPaginated])
        _webView.browsingContextController.paginationMode = WKPaginationModeUnpaginated;
    else {
        _webView.browsingContextController.paginationMode = WKPaginationModeLeftToRight;
        _webView.browsingContextController.pageLength = _webView.bounds.size.width / 2;
        _webView.browsingContextController.gapBetweenPages = 10;
    }
}

- (IBAction)toggleTransparentWindow:(id)sender
{
    BOOL isTransparent = _webView.drawsTransparentBackground;
    isTransparent = !isTransparent;

    [[self window] setOpaque:!isTransparent];
    [[self window] setHasShadow:!isTransparent];

    _webView.drawsTransparentBackground = isTransparent;

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

static void dumpSource(WKStringRef source, WKErrorRef error, void* context)
{
    if (!source)
        return;

    LOG(@"Main frame source\n \"%@\"", CFBridgingRelease(WKStringCopyCFString(0, source)));
}

- (IBAction)dumpSourceToConsole:(id)sender
{
    WKPageGetSourceForFrame(_webView.pageRef, WKPageGetMainFrame(_webView.pageRef), NULL, dumpSource);
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (context != keyValueObservingContext || object != _webView.browsingContextController)
        return;

    if ([keyPath isEqualToString:@"title"])
        self.window.title = [_webView.browsingContextController.title stringByAppendingString:_webView.isUsingUISideCompositing ? @" [WK2 UI]" : @" [WK2]"];
    else if ([keyPath isEqualToString:@"activeURL"])
        [self updateTextFieldFromURL:_webView.browsingContextController.activeURL];
}

// MARK: UI Client Callbacks

static WKPageRef createNewPage(WKPageRef page, WKURLRequestRef request, WKDictionaryRef features, WKEventModifiers modifiers, WKEventMouseButton button, const void* clientInfo)
{
    LOG(@"createNewPage");
    WK2BrowserWindowController *originator = (WK2BrowserWindowController *)clientInfo;
    WK2BrowserWindowController *controller = [[WK2BrowserWindowController alloc] initWithProcessGroup:originator->_processGroup browsingContextGroup:originator->_browsingContextGroup];
    [controller loadWindow];

    return WKRetain(controller->_webView.pageRef);
}

static void showPage(WKPageRef page, const void *clientInfo)
{
    LOG(@"showPage");
    [[(BrowserWindowController *)clientInfo window] orderFront:nil];
}

static void closePage(WKPageRef page, const void *clientInfo)
{
    LOG(@"closePage");
    WKPageClose(page);
    [[(BrowserWindowController *)clientInfo window] close];
}

static void runJavaScriptAlert(WKPageRef page, WKStringRef message, WKFrameRef frame, const void* clientInfo)
{
    NSAlert* alert = [[NSAlert alloc] init];

    WKURLRef wkURL = WKFrameCopyURL(frame);
    CFURLRef cfURL = WKURLCopyCFURL(0, wkURL);
    WKRelease(wkURL);

    [alert setMessageText:[NSString stringWithFormat:@"JavaScript alert dialog from %@.", [(NSURL *)cfURL absoluteString]]];
    CFRelease(cfURL);

    CFStringRef cfMessage = WKStringCopyCFString(0, message);
    [alert setInformativeText:(NSString *)cfMessage];
    CFRelease(cfMessage);

    [alert addButtonWithTitle:@"OK"];

    [alert runModal];
    [alert release];
}

static bool runJavaScriptConfirm(WKPageRef page, WKStringRef message, WKFrameRef frame, const void* clientInfo)
{
    NSAlert* alert = [[NSAlert alloc] init];

    WKURLRef wkURL = WKFrameCopyURL(frame);
    CFURLRef cfURL = WKURLCopyCFURL(0, wkURL);
    WKRelease(wkURL);

    [alert setMessageText:[NSString stringWithFormat:@"JavaScript confirm dialog from %@.", [(NSURL *)cfURL absoluteString]]];
    CFRelease(cfURL);

    CFStringRef cfMessage = WKStringCopyCFString(0, message);
    [alert setInformativeText:(NSString *)cfMessage];
    CFRelease(cfMessage);

    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];

    NSInteger button = [alert runModal];
    [alert release];

    return button == NSAlertFirstButtonReturn;
}

static WKStringRef runJavaScriptPrompt(WKPageRef page, WKStringRef message, WKStringRef defaultValue, WKFrameRef frame, const void* clientInfo)
{
    NSAlert* alert = [[NSAlert alloc] init];

    WKURLRef wkURL = WKFrameCopyURL(frame);
    CFURLRef cfURL = WKURLCopyCFURL(0, wkURL);
    WKRelease(wkURL);

    [alert setMessageText:[NSString stringWithFormat:@"JavaScript prompt dialog from %@.", [(NSURL *)cfURL absoluteString]]];
    CFRelease(cfURL);

    CFStringRef cfMessage = WKStringCopyCFString(0, message);
    [alert setInformativeText:(NSString *)cfMessage];
    CFRelease(cfMessage);

    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];

    NSTextField* input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 200, 24)];
    CFStringRef cfDefaultValue = WKStringCopyCFString(0, defaultValue);
    [input setStringValue:(NSString *)cfDefaultValue];
    CFRelease(cfDefaultValue);

    [alert setAccessoryView:input];

    NSInteger button = [alert runModal];

    NSString* result = nil;
    if (button == NSAlertFirstButtonReturn) {
        [input validateEditing];
        result = [input stringValue];
    }

    [alert release];

    if (!result)
        return 0;
    return WKStringCreateWithCFString((CFStringRef)result);
}

static void setStatusText(WKPageRef page, WKStringRef text, const void* clientInfo)
{
    LOG(@"setStatusText");
}

static void mouseDidMoveOverElement(WKPageRef page, WKHitTestResultRef hitTestResult, WKEventModifiers modifiers, WKTypeRef userData, const void *clientInfo)
{
    LOG(@"mouseDidMoveOverElement");
}

static WKRect getWindowFrame(WKPageRef page, const void* clientInfo)
{
    NSRect rect = [[(BrowserWindowController *)clientInfo window] frame];
    WKRect wkRect;
    wkRect.origin.x = rect.origin.x;
    wkRect.origin.y = rect.origin.y;
    wkRect.size.width = rect.size.width;
    wkRect.size.height = rect.size.height;
    return wkRect;
}

static void setWindowFrame(WKPageRef page, WKRect rect, const void* clientInfo)
{
    [[(BrowserWindowController *)clientInfo window] setFrame:NSMakeRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height) display:YES];
}

static bool runBeforeUnloadConfirmPanel(WKPageRef page, WKStringRef message, WKFrameRef frame, const void* clientInfo)
{
    NSAlert *alert = [[NSAlert alloc] init];

    WKURLRef wkURL = WKFrameCopyURL(frame);
    CFURLRef cfURL = WKURLCopyCFURL(0, wkURL);
    WKRelease(wkURL);

    [alert setMessageText:[NSString stringWithFormat:@"BeforeUnload confirm dialog from %@.", [(NSURL *)cfURL absoluteString]]];
    CFRelease(cfURL);

    CFStringRef cfMessage = WKStringCopyCFString(0, message);
    [alert setInformativeText:(NSString *)cfMessage];
    CFRelease(cfMessage);

    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];

    NSInteger button = [alert runModal];
    [alert release];

    return button == NSAlertFirstButtonReturn;
}

static void runOpenPanel(WKPageRef page, WKFrameRef frame, WKOpenPanelParametersRef parameters, WKOpenPanelResultListenerRef listener, const void* clientInfo)
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setAllowsMultipleSelection:WKOpenPanelParametersGetAllowsMultipleFiles(parameters)];

    WKRetain(listener);

    [openPanel beginSheetModalForWindow:[(BrowserWindowController *)clientInfo window] completionHandler:^(NSInteger result) {
        if (result == NSFileHandlingPanelOKButton) {
            WKMutableArrayRef fileURLs = WKMutableArrayCreate();

            NSURL *nsURL;
            for (nsURL in [openPanel URLs]) {
                WKURLRef wkURL = WKURLCreateWithCFURL((CFURLRef)nsURL);
                WKArrayAppendItem(fileURLs, wkURL);
                WKRelease(wkURL);
            }

            WKOpenPanelResultListenerChooseFiles(listener, fileURLs);

            WKRelease(fileURLs);
        } else
            WKOpenPanelResultListenerCancel(listener);
        
        WKRelease(listener);
    }];
}

- (void)awakeFromNib
{
    _webView = [[WKView alloc] initWithFrame:[containerView bounds] processGroup:_processGroup browsingContextGroup:_browsingContextGroup];

    _webView.allowsMagnification = YES;
    _webView.allowsBackForwardNavigationGestures = YES;

    [_webView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    [containerView addSubview:_webView];

    [progressIndicator bind:NSHiddenBinding toObject:_webView.browsingContextController withKeyPath:@"loading" options:@{ NSValueTransformerNameBindingOption : NSNegateBooleanTransformerName }];
    [progressIndicator bind:NSValueBinding toObject:_webView.browsingContextController withKeyPath:@"estimatedProgress" options:nil];

    [_webView.browsingContextController addObserver:self forKeyPath:@"title" options:0 context:keyValueObservingContext];
    [_webView.browsingContextController addObserver:self forKeyPath:@"activeURL" options:0 context:keyValueObservingContext];

    _webView.browsingContextController.loadDelegate = self;
    _webView.browsingContextController.policyDelegate = self;
    _webView.browsingContextController.historyDelegate = self;

    WKPageUIClientV2 uiClient = {
        { 2, self },
        0,          /* createNewPage_deprecatedForUseWithV0 */
        showPage,
        closePage,
        0,          /* takeFocus */
        0,          /* focus */
        0,          /* unfocus */
        runJavaScriptAlert,
        runJavaScriptConfirm,
        runJavaScriptPrompt,
        setStatusText,
        0,          /* mouseDidMoveOverElement_deprecatedForUseWithV0 */
        0,          /* missingPluginButtonClicked */
        0,          /* didNotHandleKeyEvent */
        0,          /* didNotHandleWheelEvent */
        0,          /* toolbarsAreVisible */
        0,          /* setToolbarsAreVisible */
        0,          /* menuBarIsVisible */
        0,          /* setMenuBarIsVisible */
        0,          /* statusBarIsVisible */
        0,          /* setStatusBarIsVisible */
        0,          /* isResizable */
        0,          /* setIsResizable */
        getWindowFrame,
        setWindowFrame,
        runBeforeUnloadConfirmPanel,
        0,          /* didDraw */
        0,          /* pageDidScroll */
        0,          /* exceededDatabaseQuota */
        runOpenPanel,
        0,          /* decidePolicyForGeolocationPermissionRequest */
        0, // headerHeight
        0, // footerHeight
        0, // drawHeader
        0, // drawFooter
        0, // printFrame
        0, // showModal
        0, // didCompleteRubberBandForMainFrame
        0, // saveDataToFileInDownloadsFolder
        0, // shouldInterruptJavaScript
        createNewPage,
        mouseDidMoveOverElement,
        0, // decidePolicyForNotificationPermissionRequest
        0, // unavailablePluginButtonClicked_deprecatedForUseWithV1
        0, // showColorPicker
        0, // hideColorPicker
        0, // unavailablePluginButtonClicked
    };
    WKPageSetPageUIClient(_webView.pageRef, &uiClient.base);
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
    WKStringRef string = WKStringCreateWithCFString((CFStringRef)[sender stringValue]);

    WKPageFindString(_webView.pageRef, string, kWKFindOptionsCaseInsensitive | kWKFindOptionsWrapAround | kWKFindOptionsShowFindIndicator | kWKFindOptionsShowOverlay, 100);
}

#pragma mark WKBrowsingContextLoadDelegate

- (void)browsingContextControllerDidStartProvisionalLoad:(WKBrowsingContextController *)sender
{
    LOG(@"didStartProvisionalLoad");
}

- (void)browsingContextControllerDidReceiveServerRedirectForProvisionalLoad:(WKBrowsingContextController *)sender
{
    LOG(@"didReceiveServerRedirectForProvisionalLoad");
}

- (void)browsingContextController:(WKBrowsingContextController *)sender didFailProvisionalLoadWithError:(NSError *)error
{
    LOG(@"didFailProvisionalLoadWithError: %@", error);
}

- (void)browsingContextControllerDidCommitLoad:(WKBrowsingContextController *)sender
{
    LOG(@"didCommitLoad");
}

- (void)browsingContextControllerDidFinishLoad:(WKBrowsingContextController *)sender
{
    LOG(@"didFinishLoad");
}

- (void)browsingContextController:(WKBrowsingContextController *)sender didFailLoadWithError:(NSError *)error
{
    LOG(@"didFailLoadWithError: %@", error);
}

- (void)browsingContextControllerDidChangeBackForwardList:(WKBrowsingContextController *)sender addedItem:(WKBackForwardListItem *)addedItem removedItems:(NSArray *)removedItems
{
    [self validateToolbar];
}

#pragma mark WKBrowsingContextLoadDelegatePrivate

- (BOOL)browsingContextController:(WKBrowsingContextController *)sender canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace
{
    LOG(@"canAuthenticateAgainstProtectionSpace: %@", protectionSpace);
    return YES;
}

- (void)browsingContextController:(WKBrowsingContextController *)sender didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    LOG(@"didReceiveAuthenticationChallenge: %@", challenge);
    [challenge.sender continueWithoutCredentialForAuthenticationChallenge:challenge];
}

#pragma mark WKBrowsingContextPolicyDelegate

- (void)browsingContextController:(WKBrowsingContextController *)browsingContext decidePolicyForNavigationAction:(NSDictionary *)actionInformation decisionHandler:(WKPolicyDecisionHandler)decisionHandler
{
    LOG(@"decidePolicyForNavigationAction");
    NSLog(@"action information %@", actionInformation);
    decisionHandler(WKPolicyDecisionAllow);
}

- (void)browsingContextController:(WKBrowsingContextController *)browsingContext decidePolicyForNewWindowAction:(NSDictionary *)actionInformation decisionHandler:(WKPolicyDecisionHandler)decisionHandler
{
    LOG(@"decidePolicyForNewWindowAction");
    decisionHandler(WKPolicyDecisionAllow);
}

- (void)browsingContextController:(WKBrowsingContextController *)browsingContext decidePolicyForResponseAction:(NSDictionary *)actionInformation decisionHandler:(WKPolicyDecisionHandler)decisionHandler
{
    decisionHandler(WKPolicyDecisionAllow);
}

#pragma mark WKBrowsingContextHistoryDelegate

- (void)browsingContextController:(WKBrowsingContextController *)browsingContextController didNavigateWithNavigationData:(WKNavigationData *)navigationData
{
    LOG(@"WKBrowsingContextHistoryDelegate - didNavigateWithNavigationData - title: %@ - url: %@", navigationData.title, navigationData.originalRequest.URL);
}

- (void)browsingContextController:(WKBrowsingContextController *)browsingContextController didPerformClientRedirectFromURL:(NSURL *)sourceURL toURL:(NSURL *)destinationURL
{
    LOG(@"WKBrowsingContextHistoryDelegate - didPerformClientRedirect - fromURL: %@ - toURL: %@", sourceURL, destinationURL);
}

- (void)browsingContextController:(WKBrowsingContextController *)browsingContextController didPerformServerRedirectFromURL:(NSURL *)sourceURL toURL:(NSURL *)destinationURL
{
    LOG(@"WKBrowsingContextHistoryDelegate - didPerformServerRedirect - fromURL: %@ - toURL: %@", sourceURL, destinationURL);
}

- (void)browsingContextController:(WKBrowsingContextController *)browsingContextController didUpdateHistoryTitle:(NSString *)title forURL:(NSURL *)URL
{
    LOG(@"browsingContextController - didUpdateHistoryTitle - title: %@ - URL: %@", title, URL);
}

@end

#endif // WK_API_ENABLED
