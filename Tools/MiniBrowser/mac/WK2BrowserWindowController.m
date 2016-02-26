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
#import "SettingsController.h"
#import <WebKit/WKFrameInfo.h>
#import <WebKit/WKNavigationDelegate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebNSURLExtras.h>

static void* keyValueObservingContext = &keyValueObservingContext;

@interface WK2BrowserWindowController () <WKNavigationDelegate, WKUIDelegate>
@end

@implementation WK2BrowserWindowController {
    WKWebViewConfiguration *_configuration;
    WKWebView *_webView;
    BOOL _zoomTextOnly;
    BOOL _isPrivateBrowsingWindow;

    BOOL _useShrinkToFit;
}

- (void)awakeFromNib
{
    _webView = [[WKWebView alloc] initWithFrame:[containerView bounds] configuration:_configuration];
    [self didChangeSettings];

    _webView.allowsMagnification = YES;
    _webView.allowsBackForwardNavigationGestures = YES;

    [_webView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    [containerView addSubview:_webView];

    [progressIndicator bind:NSHiddenBinding toObject:_webView withKeyPath:@"loading" options:@{ NSValueTransformerNameBindingOption : NSNegateBooleanTransformerName }];
    [progressIndicator bind:NSValueBinding toObject:_webView withKeyPath:@"estimatedProgress" options:nil];

    [_webView addObserver:self forKeyPath:@"title" options:0 context:keyValueObservingContext];
    [_webView addObserver:self forKeyPath:@"URL" options:0 context:keyValueObservingContext];

    _webView.navigationDelegate = self;
    _webView.UIDelegate = self;
    
    _webView._observedRenderingProgressEvents = _WKRenderingProgressEventFirstLayout
        | _WKRenderingProgressEventFirstVisuallyNonEmptyLayout
        | _WKRenderingProgressEventFirstPaintWithSignificantArea
        | _WKRenderingProgressEventFirstLayoutAfterSuppressedIncrementalRendering
        | _WKRenderingProgressEventFirstPaintAfterSuppressedIncrementalRendering;

    _zoomTextOnly = NO;
}

- (instancetype)initWithConfiguration:(WKWebViewConfiguration *)configuration
{
    if (!(self = [super initWithWindowNibName:@"BrowserWindow"]))
        return nil;

    _configuration = [configuration copy];
    _isPrivateBrowsingWindow = !_configuration.websiteDataStore.isPersistent;

    return self;
}

- (void)dealloc
{
    [_webView removeObserver:self forKeyPath:@"title"];
    [_webView removeObserver:self forKeyPath:@"URL"];
    
    [progressIndicator unbind:NSHiddenBinding];
    [progressIndicator unbind:NSValueBinding];

    [_webView release];
    [_configuration release];

    [super dealloc];
}

- (IBAction)fetch:(id)sender
{
    [urlText setStringValue:[self addProtocolIfNecessary:[urlText stringValue]]];

    [_webView loadRequest:[NSURLRequest requestWithURL:[NSURL _web_URLWithUserTypedString:[urlText stringValue]]]];
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

static CGFloat viewScaleForMenuItemTag(NSInteger tag)
{
    if (tag == 1)
        return 1;
    if (tag == 2)
        return 0.75;
    if (tag == 3)
        return 0.5;
    if (tag == 4)
        return 0.25;

    return 1;
}

- (IBAction)setScale:(id)sender
{
    CGFloat scale = viewScaleForMenuItemTag([sender tag]);
    CGFloat oldScale = [_webView _viewScale];

    if (scale == oldScale)
        return;

    [_webView _setLayoutMode:_WKLayoutModeDynamicSizeComputedFromViewScale];

    NSRect oldFrame = self.window.frame;
    NSSize newFrameSize = NSMakeSize(oldFrame.size.width * (scale / oldScale), oldFrame.size.height * (scale / oldScale));
    [self.window setFrame:NSMakeRect(oldFrame.origin.x, oldFrame.origin.y - (newFrameSize.height - oldFrame.size.height), newFrameSize.width, newFrameSize.height) display:NO animate:NO];

    [_webView _setViewScale:scale];
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
    if (action == @selector(dumpSourceToConsole:)
        || action == @selector(find:)
        || action == @selector(forceRepaint:))
        return NO;
    
    if (action == @selector(showHideWebView:))
        [menuItem setTitle:[_webView isHidden] ? @"Show Web View" : @"Hide Web View"];
    else if (action == @selector(removeReinsertWebView:))
        [menuItem setTitle:[_webView window] ? @"Remove Web View" : @"Insert Web View"];
    else if (action == @selector(toggleZoomMode:))
        [menuItem setState:_zoomTextOnly ? NSOnState : NSOffState];

    if (action == @selector(setScale:))
        [menuItem setState:[_webView _viewScale] == viewScaleForMenuItemTag([menuItem tag])];

    return YES;
}

- (IBAction)reload:(id)sender
{
    [_webView reload];
}

- (IBAction)forceRepaint:(id)sender
{
    // FIXME: This doesn't actually force a repaint.
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
    if (_zoomTextOnly) {
        _zoomTextOnly = NO;
        double currentTextZoom = _webView._textZoomFactor;
        _webView._textZoomFactor = 1;
        _webView._pageZoomFactor = currentTextZoom;
    } else {
        _zoomTextOnly = YES;
        double currentPageZoom = _webView._pageZoomFactor;
        _webView._textZoomFactor = currentPageZoom;
        _webView._pageZoomFactor = 1;
    }
}

- (IBAction)resetZoom:(id)sender
{
    if (![self canResetZoom])
        return;

    if (_zoomTextOnly)
        _webView._textZoomFactor = 1;
    else
        _webView._pageZoomFactor = 1;
}

- (BOOL)canResetZoom
{
    return _zoomTextOnly ? (_webView._textZoomFactor != 1) : (_webView._pageZoomFactor != 1);
}

- (IBAction)toggleShrinkToFit:(id)sender
{
    _useShrinkToFit = !_useShrinkToFit;
    toggleUseShrinkToFitButton.image = _useShrinkToFit ? [NSImage imageNamed:@"NSExitFullScreenTemplate"] : [NSImage imageNamed:@"NSEnterFullScreenTemplate"];
    [_webView _setLayoutMode:_useShrinkToFit ? _WKLayoutModeDynamicSizeComputedFromMinimumDocumentSize : _WKLayoutModeViewSize];
}

- (IBAction)dumpSourceToConsole:(id)sender
{
}

- (NSURL *)currentURL
{
    return _webView.URL;
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    SEL action = item.action;

    if (action == @selector(goBack:) || action == @selector(goForward:))
        return [_webView validateUserInterfaceItem:item];

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
    [(BrowserAppDelegate *)[[NSApplication sharedApplication] delegate] browserWindowWillClose:self.window];
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
    return _zoomTextOnly ? _webView._textZoomFactor : _webView._pageZoomFactor;
}

- (void)setCurrentZoomFactor:(CGFloat)factor
{
    if (_zoomTextOnly)
        _webView._textZoomFactor = factor;
    else
        _webView._pageZoomFactor = factor;
}

- (BOOL)canZoomIn
{
    return self.currentZoomFactor * DefaultZoomFactorRatio < DefaultMaximumZoomFactor;
}

- (void)zoomIn:(id)sender
{
    if (!self.canZoomIn)
        return;

    self.currentZoomFactor *= DefaultZoomFactorRatio;
}

- (BOOL)canZoomOut
{
    return self.currentZoomFactor / DefaultZoomFactorRatio > DefaultMinimumZoomFactor;
}

- (void)zoomOut:(id)sender
{
    if (!self.canZoomIn)
        return;

    self.currentZoomFactor /= DefaultZoomFactorRatio;
}

- (void)didChangeSettings
{
    SettingsController *settings = [SettingsController shared];
    WKPreferences *preferences = _webView.configuration.preferences;

    preferences._tiledScrollingIndicatorVisible = settings.tiledScrollingIndicatorVisible;
    preferences._compositingBordersVisible = settings.layerBordersVisible;
    preferences._compositingRepaintCountersVisible = settings.layerBordersVisible;
    preferences._simpleLineLayoutDebugBordersEnabled = settings.simpleLineLayoutDebugBordersEnabled;
    preferences._acceleratedDrawingEnabled = settings.acceleratedDrawingEnabled;
    preferences._resourceUsageOverlayVisible = settings.resourceUsageOverlayVisible;
    preferences._displayListDrawingEnabled = settings.displayListDrawingEnabled;

    BOOL useTransparentWindows = settings.useTransparentWindows;
    if (useTransparentWindows != !_webView._drawsBackground) {
        [self.window setOpaque:!useTransparentWindows];
        [self.window setBackgroundColor:[NSColor clearColor]];
        [self.window setHasShadow:!useTransparentWindows];

        _webView._drawsBackground = !useTransparentWindows;

        [self.window display];
    }

    BOOL usePaginatedMode = settings.usePaginatedMode;
    if (usePaginatedMode != (_webView._paginationMode != _WKPaginationModeUnpaginated)) {
        if (usePaginatedMode) {
            _webView._paginationMode = _WKPaginationModeLeftToRight;
            _webView._pageLength = _webView.bounds.size.width / 2;
            _webView._gapBetweenPages = 10;
        } else
            _webView._paginationMode = _WKPaginationModeUnpaginated;
    }
    
    NSUInteger visibleOverlayRegions = 0;
    if (settings.nonFastScrollableRegionOverlayVisible)
        visibleOverlayRegions |= _WKNonFastScrollableRegion;
    if (settings.wheelEventHandlerRegionOverlayVisible)
        visibleOverlayRegions |= _WKWheelEventHandlerRegion;
    
    preferences._visibleDebugOverlayRegions = visibleOverlayRegions;
}

- (void)updateTitle:(NSString *)title
{
    if (!title)
        title = _webView.URL.lastPathComponent;
    
    self.window.title = [NSString stringWithFormat:@"%@%@ [WK2 %d]", _isPrivateBrowsingWindow ? @"🙈 " : @"", title, _webView._webProcessIdentifier];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (context != keyValueObservingContext || object != _webView)
        return;

    if ([keyPath isEqualToString:@"title"])
        [self updateTitle:_webView.title];
    else if ([keyPath isEqualToString:@"URL"])
        [self updateTextFieldFromURL:_webView.URL];
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    NSAlert* alert = [[NSAlert alloc] init];

    [alert setMessageText:[NSString stringWithFormat:@"JavaScript alert dialog from %@.", [frame.request.URL absoluteString]]];
    [alert setInformativeText:message];
    [alert addButtonWithTitle:@"OK"];

    [alert beginSheetModalForWindow:self.window completionHandler:^void (NSModalResponse response) {
        completionHandler();
        [alert release];
    }];
}

- (void)webView:(WKWebView *)webView runJavaScriptConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL result))completionHandler
{
    NSAlert* alert = [[NSAlert alloc] init];

    [alert setMessageText:[NSString stringWithFormat:@"JavaScript confirm dialog from %@.", [frame.request.URL  absoluteString]]];
    [alert setInformativeText:message];
    
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];

    [alert beginSheetModalForWindow:self.window completionHandler:^void (NSModalResponse response) {
        completionHandler(response == NSAlertFirstButtonReturn);
        [alert release];
    }];
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
    
    [alert beginSheetModalForWindow:self.window completionHandler:^void (NSModalResponse response) {
        [input validateEditing];
        completionHandler(response == NSAlertFirstButtonReturn ? [input stringValue] : nil);
        [alert release];
    }];
}

- (void)updateTextFieldFromURL:(NSURL *)URL
{
    if (!URL)
        return;

    if (!URL.absoluteString.length)
        return;

    urlText.stringValue = [URL _web_userVisibleString];
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

static NSSet *dataTypes()
{
    return [WKWebsiteDataStore allWebsiteDataTypes];
}

- (IBAction)fetchWebsiteData:(id)sender
{
    [_configuration.websiteDataStore _fetchDataRecordsOfTypes:dataTypes() withOptions:_WKWebsiteDataStoreFetchOptionComputeSizes completionHandler:^(NSArray *websiteDataRecords) {
        NSLog(@"did fetch website data %@.", websiteDataRecords);
    }];
}

- (IBAction)fetchAndClearWebsiteData:(id)sender
{
    [_configuration.websiteDataStore fetchDataRecordsOfTypes:dataTypes() completionHandler:^(NSArray *websiteDataRecords) {
        [_configuration.websiteDataStore removeDataOfTypes:dataTypes() forDataRecords:websiteDataRecords completionHandler:^{
            [_configuration.websiteDataStore fetchDataRecordsOfTypes:dataTypes() completionHandler:^(NSArray *websiteDataRecords) {
                NSLog(@"did clear website data, after clearing data is %@.", websiteDataRecords);
            }];
        }];
    }];
}

- (IBAction)clearWebsiteData:(id)sender
{
    [_configuration.websiteDataStore removeDataOfTypes:dataTypes() modifiedSince:[NSDate distantPast] completionHandler:^{
        NSLog(@"Did clear website data.");
    }];
}

- (IBAction)printWebView:(id)sender
{
    [[_webView _printOperationWithPrintInfo:[NSPrintInfo sharedPrintInfo] forFrame:nil] runOperationModalForWindow:self.window delegate:nil didRunSelector:nil contextInfo:nil];
}

#pragma mark WKNavigationDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    LOG(@"decidePolicyForNavigationResponse");
    decisionHandler(WKNavigationResponsePolicyAllow);
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    LOG(@"didStartProvisionalNavigation: %@", navigation);
}

- (void)webView:(WKWebView *)webView didReceiveServerRedirectForProvisionalNavigation:(WKNavigation *)navigation
{
    LOG(@"didReceiveServerRedirectForProvisionalNavigation: %@", navigation);
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    LOG(@"didFailProvisionalNavigation: %@navigation, error: %@", navigation, error);
}

- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation
{
    LOG(@"didCommitNavigation: %@", navigation);
    [self updateTitle:nil];
}

- (void)webView:(WKWebView *)webView didFinishLoadingNavigation:(WKNavigation *)navigation
{
    LOG(@"didFinishLoadingNavigation: %@", navigation);
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    LOG(@"didFailNavigation: %@, error %@", navigation, error);
}

- (void)_webViewWebProcessDidCrash:(WKWebView *)webView
{
    NSLog(@"WebContent process crashed; reloading");
    [self reload:nil];
}

- (void)_webView:(WKWebView *)webView renderingProgressDidChange:(_WKRenderingProgressEvents)progressEvents
{
    if (progressEvents & _WKRenderingProgressEventFirstLayout)
        LOG(@"renderingProgressDidChange: %@", @"first layout");

    if (progressEvents & _WKRenderingProgressEventFirstVisuallyNonEmptyLayout)
        LOG(@"renderingProgressDidChange: %@", @"first visually non-empty layout");

    if (progressEvents & _WKRenderingProgressEventFirstPaintWithSignificantArea)
        LOG(@"renderingProgressDidChange: %@", @"first paint with significant area");

    if (progressEvents & _WKRenderingProgressEventFirstLayoutAfterSuppressedIncrementalRendering)
        LOG(@"renderingProgressDidChange: %@", @"first layout after suppressed incremental rendering");

    if (progressEvents & _WKRenderingProgressEventFirstPaintAfterSuppressedIncrementalRendering)
        LOG(@"renderingProgressDidChange: %@", @"first paint after suppressed incremental rendering");
}

@end

#endif // WK_API_ENABLED
