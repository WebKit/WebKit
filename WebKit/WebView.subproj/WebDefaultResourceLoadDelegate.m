/*
     WebDefaultResourceLoadDelegate.m
     Copyright 2003, Apple, Inc. All rights reserved.
*/

#import <WebFoundation/WebError.h>
#import <WebFoundation/NSURLRequest.h>
#import <WebFoundation/WebResource.h>
#import <WebFoundation/WebResponse.h>

#import <WebKit/WebDataSource.h>
#import <WebKit/WebDefaultResourceLoadDelegate.h>
#import <WebKit/WebPluginError.h>
#import <WebKit/WebView.h>


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

- webView: (WebView *)wv identifierForInitialRequest: (NSURLRequest *)request fromDataSource: (WebDataSource *)dataSource
{
    return [[[NSObject alloc] init] autorelease];
}

-(NSURLRequest *)webView: (WebView *)wv resource:identifier willSendRequest: (NSURLRequest *)newRequest fromDataSource:(WebDataSource *)dataSource
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

- (void)webView: (WebView *)wv plugInFailedWithError:(WebPlugInError *)error dataSource:(WebDataSource *)dataSource
{
}

@end


