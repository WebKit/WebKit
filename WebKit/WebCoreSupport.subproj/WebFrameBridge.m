//
//  WebFrameBridge.m
//  WebKit
//
//  Created by Darin Adler on Sun Jun 16 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebFrameBridge.h>

#import <WebFoundation/WebCacheLoaderConstants.h>

#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebView.h>
#import <WebKit/WebKitDebug.h>

@implementation WebFrameBridge

// owned by the WebFrame

- initWithWebFrame:(WebFrame *)f
{
    [super init];
    
    frame = f; // don't retain
    
    return self;
}

- (WebView *)view
{
    return [frame webView];
}

- (WebHTMLView *)HTMLView
{
    return (WebHTMLView *)[[self view] documentView];
}

- (WebCoreBridge *)bridge
{
    WebCoreBridge *bridge = [[frame provisionalDataSource] _bridge];
    if (bridge) {
        return bridge;
    }
    return [[frame dataSource] _bridge];
}

- (WebCoreBridge *)committedBridge
{
    return [[frame dataSource] _bridge];
}

- (void)loadURL:(NSURL *)URL attributes:(NSDictionary *)attributes flags:(unsigned)flags withParent:(WebDataSource *)parent
{
    WebDataSource *newDataSource = [[WebDataSource alloc] initWithURL:URL attributes:attributes flags:flags];
    [newDataSource _setParent:parent];
    if ([frame setProvisionalDataSource:newDataSource]) {
        [frame startLoading];
    }
    [newDataSource release];
}

- (void)loadURL:(NSURL *)URL
{
    [self loadURL:URL attributes:nil flags:0 withParent:[[frame dataSource] parent]];
}

- (void)postWithURL:(NSURL *)URL data:(NSData *)data
{
    // When posting, use the WebResourceHandleFlagLoadFromOrigin load flag. 
    // This prevents a potential bug which may cause a page
    // with a form that uses itself as an action to be returned 
    // from the cache without submitting.
    NSDictionary *attributes = [[NSDictionary alloc] initWithObjectsAndKeys:
        data, WebHTTPResourceHandleRequestData,
        @"POST", WebHTTPResourceHandleRequestMethod,
        nil];
    [self loadURL:URL attributes:attributes flags:WebResourceHandleFlagLoadFromOrigin withParent:[[frame dataSource] parent]];
    [attributes release];
}

@end
