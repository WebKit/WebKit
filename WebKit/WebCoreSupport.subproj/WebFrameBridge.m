//
//  IFWebCoreFrame.m
//  WebKit
//
//  Created by Darin Adler on Sun Jun 16 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/IFWebCoreFrame.h>

#import <WebFoundation/IFURLCacheLoaderConstants.h>

#import <WebKit/IFHTMLViewPrivate.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebFrame.h>
#import <WebKit/IFWebView.h>
#import <WebKit/WebKitDebug.h>

@implementation IFWebCoreFrame

// owned by the IFWebFrame

- initWithWebFrame:(IFWebFrame *)f
{
    [super init];
    
    frame = f; // don't retain
    
    return self;
}

- (IFWebView *)view
{
    return [frame webView];
}

- (IFHTMLView *)HTMLView
{
    return [[self view] documentView];
}

- (KHTMLView *)widget
{
    WEBKIT_ASSERT([self HTMLView]);
    KHTMLView *widget = [[self HTMLView] _provisionalWidget];
    if (widget) {
        return widget;
    }
    return [[self HTMLView] _widget];
}

- (void)loadURL:(NSURL *)URL attributes:(NSDictionary *)attributes flags:(unsigned)flags withParent:(IFWebDataSource *)parent
{
    IFWebDataSource *newDataSource = [[IFWebDataSource alloc] initWithURL:URL attributes:attributes flags:flags];
    [newDataSource _setParent:parent];
    [frame setProvisionalDataSource:newDataSource];
    [newDataSource release];
    [frame startLoading];
}

- (void)loadURL:(NSURL *)URL
{
    [self loadURL:URL attributes:nil flags:0 withParent:[[frame dataSource] parent]];
}

- (void)postWithURL:(NSURL *)URL data:(NSData *)data
{
    // When posting, use the IFURLHandleFlagLoadFromOrigin load flag. 
    // This prevents a potential bug which may cause a page
    // with a form that uses itself as an action to be returned 
    // from the cache without submitting.
    NSDictionary *attributes = [[NSDictionary alloc] initWithObjectsAndKeys:
        data, IFHTTPURLHandleRequestData,
        @"POST", IFHTTPURLHandleRequestMethod,
        nil];
    [self loadURL:URL attributes:attributes flags:IFURLHandleFlagLoadFromOrigin withParent:[[frame dataSource] parent]];
    [attributes release];
}

@end
