/*
    WebUIDelegatePrivate.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <WebKit/WebUIDelegate.h>

// FIXME: These should move to WebUIDelegate.h as part of the WebMenuItemTag enum there, when we're not in API freeze
enum {
    WebMenuItemTagSearchInSpotlight=1000,
    WebMenuItemTagSearchInGoogle,
    WebMenuItemTagLookUpInDictionary,
};

@interface NSObject (WebUIDelegatePrivate)

// webViewPrint: is obsolete; delegates should respond to webView:printFrameView: instead
- (void)webViewPrint:(WebView *)sender;
- (void)webView:(WebView *)sender printFrameView:(WebFrameView *)frameView;

- (float)webViewHeaderHeight:(WebView *)sender;
- (float)webViewFooterHeight:(WebView *)sender;
- (void)webView:(WebView *)sender drawHeaderInRect:(NSRect)rect;
- (void)webView:(WebView *)sender drawFooterInRect:(NSRect)rect;
- (void)webView:(WebView *)webView addMessageToConsole:(NSDictionary *)message;

// regions is an dictionary whose keys are regions label and values are arrays of WebDashboardRegions.
- (void)webView:(WebView *)webView dashboardRegionsChanged:(NSDictionary *)regions;

- (WebView *)webView:(WebView *)sender createWebViewModalDialogWithRequest:(NSURLRequest *)request;
- (void)webViewRunModal:(WebView *)sender;

@end
