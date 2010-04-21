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

#import "BrowserWindowController.h"

#import <WebKit2/WKStringCF.h>
#import <WebKit2/WKURLCF.h>

@interface BrowserWindowController ()
- (void)didStartProgress;
- (void)didChangeProgress:(double)value;
- (void)didFinishProgress;
@end

@implementation BrowserWindowController

- (id)initWithPageNamespace:(WKPageNamespaceRef)pageNamespace
{
    if (self = [super initWithWindowNibName:@"BrowserWindow"])
        _pageNamespace = WKPageNamespaceRetain(pageNamespace);
    
    return self;
}

- (void)dealloc
{
    assert(!_pageNamespace);
    [super dealloc];
}

- (IBAction)fetch:(id)sender
{
    CFURLRef cfURL = CFURLCreateWithString(0, (CFStringRef)[urlText stringValue], 0);
    WKURLRef url = WKURLCreateWithCFURL(cfURL);
    CFRelease(cfURL);

    WKPageLoadURL(_webView.pageRef, url);
    WKURLRelease(url);
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
    if ([menuItem action] == @selector(showHideWebView:))
        [menuItem setTitle:[_webView isHidden] ? @"Show Web View" : @"Hide Web View"];
    else if ([menuItem action] == @selector(removeReinsertWebView:))
        [menuItem setTitle:[_webView window] ? @"Remove Web View" : @"Insert Web View"];
    return YES;
}

- (IBAction)reload:(id)sender
{
    WKPageReload(_webView.pageRef);
}

- (IBAction)forceRepaint:(id)sender
{
    [_webView setNeedsDisplay:YES];
}

- (BOOL)windowShouldClose:(id)sender
{
    NSLog(@"windowShouldClose");
    BOOL canCloseImmediately = WKPageTryClose(_webView.pageRef);
    return canCloseImmediately;
}

- (void)windowWillClose:(NSNotification *)notification
{
    WKPageNamespaceRelease(_pageNamespace);
    _pageNamespace = 0;
}

- (void)applicationTerminating
{
    WKPageClose(_webView.pageRef);
    WKPageRelease(_webView.pageRef);
}

#pragma mark Loader Client Callbacks

static void _didStartProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, const void *clientInfo)
{
    NSLog(@"didStartProvisionalLoadForFrame");
}

static void _didReceiveServerRedirectForProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, const void *clientInfo)
{
    NSLog(@"didReceiveServerRedirectForProvisionalLoadForFrame");
}

static void _didFailProvisionalLoadWithErrorForFrame(WKPageRef page, WKFrameRef frame, const void *clientInfo)
{
    NSLog(@"didFailProvisionalLoadWithErrorForFrame");
}

static void _didCommitLoadForFrame(WKPageRef page, WKFrameRef frame, const void *clientInfo)
{
    NSLog(@"didCommitLoadForFrame");
}

static void _didFinishLoadForFrame(WKPageRef page, WKFrameRef frame, const void *clientInfo)
{
    NSLog(@"didFinishLoadForFrame");
}

static void _didFailLoadWithErrorForFrame(WKPageRef page, WKFrameRef frame, const void *clientInfo)
{
    NSLog(@"didFailLoadWithErrorForFrame");
}

static void _didReceiveTitleForFrame(WKPageRef page, WKStringRef title, WKFrameRef frame, const void *clientInfo)
{
    CFStringRef cfTitle = WKStringCopyCFString(0, title);
    NSLog(@"didReceiveTitleForFrame \"%@\"", (NSString *)cfTitle);
    CFRelease(cfTitle);
}

static void _didFirstLayoutForFrame(WKPageRef page, WKFrameRef frame, const void *clientInfo)
{
    NSLog(@"didFirstLayoutForFrame");
}

static void _didFirstVisuallyNonEmptyLayoutForFrame(WKPageRef page, WKFrameRef frame, const void *clientInfo)
{
    NSLog(@"didFirstVisuallyNonEmptyLayoutForFrame");
}

static void _didStartProgress(WKPageRef page, const void *clientInfo)
{
    [(BrowserWindowController *)clientInfo didStartProgress];
}

static void _didChangeProgress(WKPageRef page, double value, const void *clientInfo)
{
    [(BrowserWindowController *)clientInfo didChangeProgress:value];
}

static void _didFinishProgress(WKPageRef page, const void *clientInfo)
{
    [(BrowserWindowController *)clientInfo didFinishProgress];
}

static void _didBecomeUnresponsive(WKPageRef page, const void *clientInfo)
{
    NSLog(@"didBecomeUnresponsive");
}

static void _didBecomeResponsive(WKPageRef page, const void *clientInfo)
{
    NSLog(@"didBecomeResponsive");
}

#pragma mark Policy Client Callbacks

static void _decidePolicyForNavigationAction(WKPageRef page, uint32_t navigationType, WKURLRef url, WKFrameRef frame, WKFramePolicyListenerRef listener, const void *clientInfo)
{
    NSLog(@"decidePolicyForNavigationAction");
    WKFramePolicyListenerUse(listener);
}

static void _decidePolicyForNewWindowAction(WKPageRef page, uint32_t navigationType, WKURLRef url, WKFrameRef frame, WKFramePolicyListenerRef listener, const void *clientInfo)
{
    NSLog(@"decidePolicyForNewWindowAction");
    WKFramePolicyListenerUse(listener);
}

static void _decidePolicyForMIMEType(WKPageRef page, WKStringRef MIMEType, WKURLRef url, WKFrameRef frame, WKFramePolicyListenerRef listener, const void *clientInfo)
{
    WKFramePolicyListenerUse(listener);
}

#pragma mark UI Client Callbacks

static WKPageRef _createNewPage(WKPageRef page, const void* clientInfo)
{
    NSLog(@"createNewPage");
    BrowserWindowController *controller = [[BrowserWindowController alloc] initWithPageNamespace:WKPageGetPageNamespace(page)];
    [controller loadWindow];

    return controller->_webView.pageRef;
}

static void _showPage(WKPageRef page, const void *clientInfo)
{
    NSLog(@"showPage");
    [[(BrowserWindowController *)clientInfo window] orderFront:nil];
}

static void _closePage(WKPageRef page, const void *clientInfo)
{
    NSLog(@"closePage");
    WKPageClose(page);
    [[(BrowserWindowController *)clientInfo window] close];
    WKPageRelease(page);
}

static void _runJavaScriptAlert(WKPageRef page, WKStringRef alertText, WKFrameRef frame, const void* clientInfo)
{
    NSAlert* alert = [[NSAlert alloc] init];

    CFURLRef cfURL = WKURLCopyCFURL(0, WKFrameGetURL(frame));
    [alert setMessageText:[NSString stringWithFormat:@"JavaScript Alert from %@.", [(NSURL *)cfURL absoluteString]]];
    CFRelease(cfURL);

    CFStringRef cfAlertText = WKStringCopyCFString(0, alertText);
    [alert setInformativeText:(NSString *)alertText];
    CFRelease(cfAlertText);

    [alert addButtonWithTitle:@"OK"];

    [alert runModal];
    [alert release];
}


#pragma mark History Client Callbacks

static void _didNavigateWithNavigationData(WKPageRef page, WKNavigationDataRef navigationData, WKFrameRef frame, const void *clientInfo)
{
    CFStringRef title = WKStringCopyCFString(0, WKNavigationDataGetTitle(navigationData));
    CFURLRef url = WKURLCopyCFURL(0, WKNavigationDataGetURL(navigationData));
    NSLog(@"HistoryClient - didNavigateWithNavigationData - title: %@ - url: %@", title, url);
    CFRelease(title);
    CFRelease(url);
}

static void _didPerformClientRedirect(WKPageRef page, WKURLRef sourceURL, WKURLRef destinationURL, WKFrameRef frame, const void *clientInfo)
{
    CFURLRef cfSourceURL = WKURLCopyCFURL(0, sourceURL);
    CFURLRef cfDestinationURL = WKURLCopyCFURL(0, destinationURL);
    NSLog(@"HistoryClient - didPerformClientRedirect - sourceURL: %@ - destinationURL: %@", cfSourceURL, cfDestinationURL);
    CFRelease(cfSourceURL);
    CFRelease(cfDestinationURL);
}

static void _didPerformServerRedirect(WKPageRef page, WKURLRef sourceURL, WKURLRef destinationURL, WKFrameRef frame, const void *clientInfo)
{
    CFURLRef cfSourceURL = WKURLCopyCFURL(0, sourceURL);
    CFURLRef cfDestinationURL = WKURLCopyCFURL(0, destinationURL);
    NSLog(@"HistoryClient - didPerformServerRedirect - sourceURL: %@ - destinationURL: %@", cfSourceURL, cfDestinationURL);
    CFRelease(cfSourceURL);
    CFRelease(cfDestinationURL);
}

static void _didUpdateHistoryTitle(WKPageRef page, WKStringRef title, WKURLRef URL, WKFrameRef frame, const void *clientInfo)
{
    CFStringRef cfTitle = WKStringCopyCFString(0, title);
    CFURLRef cfURL = WKURLCopyCFURL(0, URL);
    NSLog(@"HistoryClient - didUpdateHistoryTitle - title: %@ - URL: %@", cfTitle, cfURL);
    CFRelease(cfTitle);
    CFRelease(cfURL);
}

- (void)awakeFromNib
{
    _webView = [[WKView alloc] initWithFrame:[containerView frame] pageNamespaceRef:_pageNamespace];

    [containerView addSubview:_webView];
    [_webView setFrame:[containerView frame]];
    
    [_webView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    
    WKPageLoaderClient loadClient = {
        0,      /* version */
        self,   /* clientInfo */
        _didStartProvisionalLoadForFrame,
        _didReceiveServerRedirectForProvisionalLoadForFrame,
        _didFailProvisionalLoadWithErrorForFrame,
        _didCommitLoadForFrame,
        _didFinishLoadForFrame,
        _didFailLoadWithErrorForFrame,
        _didReceiveTitleForFrame,
        _didFirstLayoutForFrame,
        _didFirstVisuallyNonEmptyLayoutForFrame,
        _didStartProgress,
        _didChangeProgress,
        _didFinishProgress,
        _didBecomeUnresponsive,
        _didBecomeResponsive
    };
    WKPageSetPageLoaderClient(_webView.pageRef, &loadClient);
    
    WKPagePolicyClient policyClient = {
        0,          /* version */
        self,       /* clientInfo */
        _decidePolicyForNavigationAction,
        _decidePolicyForNewWindowAction,
        _decidePolicyForMIMEType
    };
    WKPageSetPagePolicyClient(_webView.pageRef, &policyClient);

    WKPageUIClient uiClient = {
        0,          /* version */
        self,       /* clientInfo */
        _createNewPage,
        _showPage,
        _closePage,
        _runJavaScriptAlert
    };
    WKPageSetPageUIClient(_webView.pageRef, &uiClient);

    WKPageHistoryClient historyClient = {
        0,
        self,
        _didNavigateWithNavigationData,
        _didPerformClientRedirect,
        _didPerformServerRedirect,
        _didUpdateHistoryTitle,
    };

    WKPageSetPageHistoryClient(_webView.pageRef, &historyClient);
}

- (void)didStartProgress
{
    [progressIndicator setDoubleValue:0.0];
    [progressIndicator setHidden:NO];
}

- (void)didChangeProgress:(double)value
{
    [progressIndicator setDoubleValue:value];
}

- (void)didFinishProgress
{
    [progressIndicator setHidden:YES];
    [progressIndicator setDoubleValue:1.0];
}

- (void)loadURLString:(NSString *)urlString
{
    // FIXME: We shouldn't have to set the url text here.
    [urlText setStringValue:urlString];
    [self fetch:nil];
}

@end
