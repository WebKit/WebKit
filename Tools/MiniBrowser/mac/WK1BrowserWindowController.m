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

#import "WK1BrowserWindowController.h"

#import "AppDelegate.h"
#import "SettingsController.h"
#import <WebKit/WebInspector.h>
#import <WebKit/WebKit.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebPreferenceKeysPrivate.h>
#import <WebKit/WebViewPrivate.h>

@interface WK1BrowserWindowController () <WebFrameLoadDelegate, WebPolicyDelegate, WebResourceLoadDelegate, WebUIDelegate>
@end

@implementation WK1BrowserWindowController

- (void)awakeFromNib
{
    _webView = [[WebView alloc] initWithFrame:[containerView bounds] frameName:nil groupName:@"MiniBrowser"];
    [_webView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];

    _webView.editable = self.isEditable;

    [_webView setFrameLoadDelegate:self];
    [_webView setUIDelegate:self];
    [_webView setResourceLoadDelegate:self];
    [_webView setPolicyDelegate:self];

    [[WebPreferences standardPreferences] setFullScreenEnabled:YES];
    [[WebPreferences standardPreferences] setDeveloperExtrasEnabled:YES];
    [[WebPreferences standardPreferences] setImageControlsEnabled:YES];
    [[WebPreferences standardPreferences] setServiceControlsEnabled:YES];
    [[WebPreferences standardPreferences] setJavaScriptCanOpenWindowsAutomatically:YES];

    [_webView _listenForLayoutMilestones:WebDidFirstLayout | WebDidFirstVisuallyNonEmptyLayout | WebDidHitRelevantRepaintedObjectsAreaThreshold];

    SettingsController *settingsController = [[NSApplication sharedApplication] browserAppDelegate].settingsController;
    if (settingsController.customUserAgent)
        _webView.customUserAgent = settingsController.customUserAgent;

    [self didChangeSettings];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(userAgentDidChange:) name:kUserAgentChangedNotificationName object:nil];

    [containerView addSubview:_webView];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_webView setFrameLoadDelegate:nil];
    [_webView setUIDelegate:nil];
    [_webView setResourceLoadDelegate:nil];
}

- (void)userAgentDidChange:(NSNotification *)notification
{
    SettingsController *settingsController = [[NSApplication sharedApplication] browserAppDelegate].settingsController;
    _webView.customUserAgent = settingsController.customUserAgent;
    [_webView reload:nil];
}

- (void)loadURLString:(NSString *)urlString
{
    // FIXME: We shouldn't have to set the url text here.
    [urlText setStringValue:urlString];
    [self fetch:nil];
}

- (void)loadHTMLString:(NSString *)HTMLString
{
    [_webView.mainFrame loadHTMLString:HTMLString baseURL:nil];
}

- (IBAction)fetch:(id)sender
{
    [urlText setStringValue:[self addProtocolIfNecessary:urlText.stringValue]];
    NSURL *url = [NSURL _webkit_URLWithUserTypedString:urlText.stringValue];
    [[_webView mainFrame] loadRequest:[NSURLRequest requestWithURL:url]];
}

- (IBAction)setPageScale:(id)sender
{
    CGFloat scale = [self pageScaleForMenuItemTag:[sender tag]];
    [_webView _scaleWebView:scale atOrigin:NSZeroPoint];
}

- (IBAction)setViewScale:(id)sender
{
}

- (IBAction)reload:(id)sender
{
    [_webView reload:sender];
}

- (IBAction)forceRepaint:(id)sender
{
    [_webView setNeedsDisplay:YES];
}

- (IBAction)goBack:(id)sender
{
    [_webView goBack:sender];
}

- (IBAction)goForward:(id)sender
{
    [_webView goForward:sender];
}

static BOOL areEssentiallyEqual(double a, double b)
{
    double tolerance = 0.001;
    return (fabs(a - b) <= tolerance);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-implementations"
- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
#pragma GCC diagnostic pop
{
    SEL action = [menuItem action];

    if (action == @selector(saveAsPDF:))
        return NO;
    if (action == @selector(saveAsWebArchive:))
        return NO;

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
    else if (action == @selector(toggleFullWindowWebView:))
        [menuItem setTitle:[self webViewFillsWindow] ? @"Inset Web View" : @"Fit Web View to Window"];
    else if (action == @selector(toggleZoomMode:))
        [menuItem setState:_zoomTextOnly ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(toggleEditable:))
        [menuItem setState:self.isEditable ? NSControlStateValueOn : NSControlStateValueOff];
    else if (action == @selector(showHideWebInspector:))
        [menuItem setTitle:_webView.inspector.isOpen ? @"Close Web Inspector" : @"Show Web Inspector"];
    else if (action == @selector(toggleAlwaysShowsHorizontalScroller:))
        menuItem.state = _webView.alwaysShowHorizontalScroller ? NSControlStateValueOn : NSControlStateValueOff;
    else if (action == @selector(toggleAlwaysShowsVerticalScroller:))
        menuItem.state = _webView.alwaysShowVerticalScroller ? NSControlStateValueOn : NSControlStateValueOff;

    if (action == @selector(setPageScale:))
        [menuItem setState:areEssentiallyEqual([_webView _viewScaleFactor], [self pageScaleForMenuItemTag:[menuItem tag]])];

    return YES;
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    SEL action = [item action];

    if (action == @selector(goBack:))
        return [_webView canGoBack];

    if (action == @selector(goForward:))
        return [_webView canGoForward];

    if (action == @selector(showCertificate:))
        return NO;

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
    [[[NSApplication sharedApplication] browserAppDelegate] browserWindowWillClose:self.window];
}

- (double)currentZoomFactor
{
    return 1;
}

- (BOOL)canZoomIn
{
    return [_webView canMakeTextLarger];
}

- (void)zoomIn:(id)sender
{
    if (![self canZoomIn])
        return;

    if (_zoomTextOnly)
        [_webView makeTextLarger:sender];
    else
        [_webView zoomPageIn:sender];

}

- (BOOL)canZoomOut
{
    return [_webView canMakeTextSmaller];
}

- (void)zoomOut:(id)sender
{
    if (![self canZoomIn])
        return;

    if (_zoomTextOnly)
        [_webView makeTextSmaller:sender];
    else
        [_webView zoomPageOut:sender];
}

- (BOOL)canResetZoom
{
    return _zoomTextOnly ? [_webView canMakeTextStandardSize] : [_webView canResetPageZoom];
}

- (void)resetZoom:(id)sender
{
    if (![self canResetZoom])
        return;

    if (_zoomTextOnly)
        [_webView makeTextStandardSize:sender];
    else
        [_webView resetPageZoom:sender];
}

- (IBAction)toggleZoomMode:(id)sender
{
    // FIXME: non-text zoom not implemented.
    _zoomTextOnly = !_zoomTextOnly;
}

- (IBAction)toggleShrinkToFit:(id)sender
{
}

- (IBAction)dumpSourceToConsole:(id)sender
{
}

- (IBAction)showHideWebInspector:(id)sender
{
    WebInspector *inspector = _webView.inspector;
    if (inspector.isOpen)
        [inspector close:sender];
    else
        [inspector show:sender];
}

- (IBAction)toggleAlwaysShowsHorizontalScroller:(id)sender
{
    _webView.alwaysShowHorizontalScroller = !_webView.alwaysShowHorizontalScroller;
}

- (IBAction)toggleAlwaysShowsVerticalScroller:(id)sender
{
    _webView.alwaysShowVerticalScroller = !_webView.alwaysShowVerticalScroller;
}

- (NSURL *)currentURL
{
    return _webView.mainFrame.dataSource.request.URL;
}

- (NSView *)mainContentView
{
    return _webView;
}

- (void)setEditable:(BOOL)editable
{
    [super setEditable:editable];
    _webView.editable = editable;
}

- (void)didChangeSettings
{
    SettingsController *settings = [[NSApplication sharedApplication] browserAppDelegate].settingsController;

    [[WebPreferences standardPreferences] setShowDebugBorders:settings.layerBordersVisible];
    [[WebPreferences standardPreferences] setLegacyLineLayoutVisualCoverageEnabled:settings.legacyLineLayoutVisualCoverageEnabled];
    [[WebPreferences standardPreferences] setShowRepaintCounter:settings.layerBordersVisible];
    [[WebPreferences standardPreferences] setSuppressesIncrementalRendering:settings.incrementalRenderingSuppressed];
    [[WebPreferences standardPreferences] setAcceleratedDrawingEnabled:settings.acceleratedDrawingEnabled];
    [[WebPreferences standardPreferences] setResourceLoadStatisticsEnabled:settings.resourceLoadStatisticsEnabled];
    [[WebPreferences standardPreferences] setLargeImageAsyncDecodingEnabled:settings.largeImageAsyncDecodingEnabled];
    [[WebPreferences standardPreferences] setAnimatedImageAsyncDecodingEnabled:settings.animatedImageAsyncDecodingEnabled];
    [[WebPreferences standardPreferences] setColorFilterEnabled:settings.appleColorFilterEnabled];
    [[WebPreferences standardPreferences] setPunchOutWhiteBackgroundsInDarkMode:settings.punchOutWhiteBackgroundsInDarkMode];

    _webView._useSystemAppearance = settings.useSystemAppearance;

    [self setWebViewFillsWindow:settings.webViewFillsWindow];

    BOOL useTransparentWindows = settings.useTransparentWindows;
    if (useTransparentWindows != !self.window.isOpaque) {
        [self.window setOpaque:!useTransparentWindows];
        [self.window setBackgroundColor:[NSColor clearColor]];
        [self.window setHasShadow:!useTransparentWindows];

        [_webView setBackgroundColor:useTransparentWindows ? [NSColor clearColor] : [NSColor whiteColor]];

        [self.window display];
    }

    BOOL usePaginatedMode = settings.usePaginatedMode;
    if (usePaginatedMode != (_webView._paginationMode != WebPaginationModeUnpaginated)) {
        if (usePaginatedMode) {
            [_webView _setPaginationMode:WebPaginationModeLeftToRight];
            [_webView _setPageLength:_webView.bounds.size.width / 2];
            [_webView _setGapBetweenPages:10];
        } else
            [_webView _setPaginationMode:WebPaginationModeUnpaginated];
    }
}

- (IBAction)printWebView:(id)sender
{
    [[[[_webView mainFrame] frameView] printOperationWithPrintInfo:[NSPrintInfo sharedPrintInfo]] runOperationModalForWindow:self.window delegate:nil didRunSelector:nil contextInfo:nil];
}

- (void)webView:(WebView *)sender didLayout:(WebLayoutMilestones)milestones
{
    if (milestones & WebDidFirstLayout)
        LOG(@"layout milestone: %@", @"first layout");

    if (milestones & WebDidFirstVisuallyNonEmptyLayout)
        LOG(@"layout milestone: %@", @"first non-empty layout");

    if (milestones & WebDidHitRelevantRepaintedObjectsAreaThreshold)
        LOG(@"layout milestone: %@", @"relevant repainted objects area threshold");
}

- (void)webView:(WebView *)webView decidePolicyForNavigationAction:(NSDictionary *)actionInformation request:(NSURLRequest *)request frame:(WebFrame *)frame decisionListener:(id<WebPolicyDecisionListener>)listener
{
    [listener use];
}

// WebFrameLoadDelegate Methods
- (void)webView:(WebView *)sender didStartProvisionalLoadForFrame:(WebFrame *)frame
{
}

- (void)updateTitle:(NSString *)title
{
    if (!title) {
        NSURL *url = _webView.mainFrame.dataSource.request.URL;
        title = url.lastPathComponent ?: url._web_userVisibleString;
    }

    [self.window setTitle:[title stringByAppendingFormat:@" [WK1]%@", _webView.editable ? @" [Editable]" : @""]];
}

- (WebView *)webView:(WebView *)sender createWebViewWithRequest:(NSURLRequest *)request
{
    WK1BrowserWindowController *newBrowserWindowController = [[WK1BrowserWindowController alloc] initWithWindowNibName:@"BrowserWindow"];
    [newBrowserWindowController.window makeKeyAndOrderFront:self];

    return newBrowserWindowController->_webView;
}

- (void)webView:(WebView *)sender didCommitLoadForFrame:(WebFrame *)frame
{
    if (frame != [sender mainFrame])
        return;

    NSURL *committedURL = [[[frame dataSource] request] URL];
    urlText.stringValue = committedURL._web_userVisibleString;

    [self updateTitle:nil];
}

- (void)webView:(WebView *)sender didChangeLocationWithinPageForFrame:(WebFrame *)frame
{
    if (frame != [sender mainFrame])
        return;

    NSURL *committedURL = [[[frame dataSource] request] URL];
    urlText.stringValue = committedURL._web_userVisibleString;
}

- (void)webView:(WebView *)sender didReceiveTitle:(NSString *)title forFrame:(WebFrame *)frame
{
    if (frame != [sender mainFrame])
        return;

    [self updateTitle:title];
}

- (NSArray *)webView:(WebView *)sender contextMenuItemsForElement:(NSDictionary *)element defaultMenuItems:(NSArray *)defaultMenuItems
{
    return defaultMenuItems;
}

- (void)webView:(WebView *)sender runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    NSAlert *alert = [[NSAlert alloc] init];
    [alert addButtonWithTitle:@"OK"];

    alert.messageText = [NSString stringWithFormat:@"JavaScript alert dialog from %@.", frame.dataSource.request.URL.absoluteString];
    alert.informativeText = message;

    [alert runModal];
}

- (BOOL)webView:(WebView *)sender runBeforeUnloadConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    NSAlert *alert = [[NSAlert alloc] init];

    alert.messageText = [NSString stringWithFormat:@"JavaScript before unload dialog from %@.", frame.dataSource.request.URL.absoluteString];
    alert.informativeText = message;

    [alert addButtonWithTitle:@"Leave Page"];
    [alert addButtonWithTitle:@"Stay On Page"];

    NSModalResponse response = [alert runModal];
    return response == NSAlertFirstButtonReturn;
}

- (NSUInteger)webView:(WebView *)webView dragDestinationActionMaskForDraggingInfo:(id <NSDraggingInfo>)draggingInfo
{
    return WebDragDestinationActionAny;
}

@end
