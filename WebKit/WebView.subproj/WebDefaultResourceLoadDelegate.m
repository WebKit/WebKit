/*
     WebDefaultResourceLoadDelegate.m
     Copyright 2003, Apple, Inc. All rights reserved.
*/

#import <WebFoundation/WebError.h>
#import <WebFoundation/WebRequest.h>
#import <WebFoundation/WebResource.h>
#import <WebFoundation/WebResponse.h>

#import <WebKit/WebDataSource.h>
#import <WebKit/WebDefaultResourceLoadDelegate.h>
#import <WebKit/WebPluginError.h>
#import <WebKit/WebController.h>


@implementation WebDefaultResourceLoadDelegate

static WebDefaultResourceLoadDelegate *sharedDelegate = nil;

// Return a object with vanilla implementations of the protocol's methods
// Note this feature relies on our default delegate being stateless
+ (WebDefaultResourceLoadDelegate *)sharedResourceLoadDelegate
{
    if (!sharedDelegate) {
        sharedDelegate = [[WebDefaultResourceLoadDelegate alloc] init];
    }
    return sharedDelegate;
}

- webView: (WebView *)wv identifierForInitialRequest: (WebRequest *)request fromDataSource: (WebDataSource *)dataSource
{
    return [[[NSObject alloc] init] autorelease];
}

-(WebRequest *)webView: (WebView *)wv resource:identifier willSendRequest: (WebRequest *)newRequest fromDataSource:(WebDataSource *)dataSource
{
    return newRequest;
}

-(void)webView: (WebView *)wv resource:identifier didReceiveResponse: (WebResponse *)response fromDataSource:(WebDataSource *)dataSource
{
}

-(void)webView: (WebView *)wv resource:identifier didReceiveContentLength: (unsigned)length fromDataSource:(WebDataSource *)dataSource
{
}

-(void)webView: (WebView *)wv resource:identifier didFinishLoadingFromDataSource:(WebDataSource *)dataSource
{
}

-(void)webView: (WebView *)wv resource:identifier didFailLoadingWithError:(WebError *)error fromDataSource:(WebDataSource *)dataSource
{
}

- (void)webView: (WebView *)wv pluginFailedWithError:(WebPluginError *)error dataSource:(WebDataSource *)dataSource
{
}

@end


