/*	
    WebBaseResourceHandleDelegate.m
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebBaseResourceHandleDelegate.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebError.h>
#import <WebFoundation/WebHTTPResourceRequest.h>
#import <WebFoundation/WebResourceHandlePrivate.h>
#import <WebFoundation/WebResourceRequest.h>
#import <WebFoundation/WebResourceResponse.h>

#import <WebKit/WebController.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebStandardPanelsPrivate.h>

@implementation WebBaseResourceHandleDelegate

- (void)_releaseResources
{
    // It's possible that when we release the handle, it will be
    // deallocated and release the last reference to this object.
    // We need to retain to avoid accessing the object after it
    // has been deallocated and also to avoid reentering this method.
    
    [self retain];
    
    [identifier release];
    identifier = nil;

    [handle release];
    handle = nil;

    [dataSource release];
    dataSource = nil;
    
    [resourceLoadDelegate release];
    resourceLoadDelegate = nil;

    [downloadDelegate release];
    downloadDelegate = nil;
    
    reachedTerminalState = YES;
    
    [self release];
}

- (void)dealloc
{
    [self _releaseResources];
    [request release];
    [response release];
    [currentURL release];
    [super dealloc];
}

- (void)loadWithRequest:(WebResourceRequest *)r
{
    ASSERT(handle == nil);
    
    handle = [[WebResourceHandle alloc] initWithRequest:r];
    if (defersCallbacks) {
        [handle _setDefersCallbacks:YES];
    }
    [handle loadWithDelegate:self];
}

- (void)setDefersCallbacks:(BOOL)defers
{
    defersCallbacks = defers;
    [handle _setDefersCallbacks:defers];
}

- (void)setDataSource: (WebDataSource *)d
{
    if (d != dataSource){
        [dataSource release];
        dataSource = [d retain];
    }
    
    if (resourceLoadDelegate != [[dataSource controller] resourceLoadDelegate]){
        [resourceLoadDelegate release];
        resourceLoadDelegate = [[[dataSource controller] resourceLoadDelegate] retain];
    }

    if (downloadDelegate != [[dataSource controller] downloadDelegate]){
        [downloadDelegate release];
        downloadDelegate = [[[dataSource controller] downloadDelegate] retain];
    }    
}

- (WebDataSource *)dataSource
{
    return dataSource;
}

- (id <WebResourceLoadDelegate>)resourceLoadDelegate
{
    return resourceLoadDelegate;
}

- (id <WebResourceLoadDelegate>)downloadDelegate
{
    return downloadDelegate;
}

- (void)setIsDownload: (BOOL)f
{
    isDownload = f;
}

- (BOOL)isDownload
{
    return isDownload;
}

-(WebResourceRequest *)handle:(WebResourceHandle *)h willSendRequest:(WebResourceRequest *)newRequest
{
    ASSERT(handle == h);
    ASSERT(!reachedTerminalState);

    [newRequest setUserAgent:[[dataSource controller] userAgentForURL:[newRequest URL]]];

    // No need to retain here, will be copied after delegate callback.
    [request autorelease];
    request = newRequest;

    if (identifier == nil){
        // The identifier is released after the last callback, rather than in dealloc
        // to avoid potential cycles.
        identifier = [[resourceLoadDelegate identifierForInitialRequest: request fromDataSource: dataSource] retain];
    }

    if (resourceLoadDelegate)
        request = [resourceLoadDelegate resource: identifier willSendRequest: request fromDataSource: dataSource];

    [[WebStandardPanels sharedStandardPanels] _didStopLoadingURL:currentURL inController: [dataSource controller]];
    
    if ([request URL] != currentURL){
        [currentURL release];
        currentURL = [[request URL] retain];
    }
    
    [[WebStandardPanels sharedStandardPanels] _didStartLoadingURL:currentURL inController:[dataSource controller]];

    // It'd be nice if we could depend on WebResourceRequest being immutable, but we can't so always
    // copy.
    request = [request copy];
    return request;
}

-(void)handle:(WebResourceHandle *)h didReceiveResponse:(WebResourceResponse *)r
{
    ASSERT(handle == h);
    ASSERT(!reachedTerminalState);

    [r retain];
    [response release];
    response = r;

    if (isDownload)
        [downloadDelegate resource:identifier didReceiveResponse:r fromDataSource: dataSource];
    else
        [resourceLoadDelegate resource:identifier didReceiveResponse:r fromDataSource: dataSource];
}

- (void)handle:(WebResourceHandle *)h didReceiveData:(NSData *)data
{
    ASSERT(handle == h);
    ASSERT(!reachedTerminalState);

    if ([self isDownload])
        [downloadDelegate resource: identifier didReceiveContentLength: [data length] fromDataSource: dataSource];
    else
        [resourceLoadDelegate resource: identifier didReceiveContentLength: [data length] fromDataSource: dataSource];
}

- (void)handleDidFinishLoading:(WebResourceHandle *)h
{
    ASSERT(handle == h);
    ASSERT(!reachedTerminalState);

    if ([self isDownload])
        [downloadDelegate resource:identifier didFinishLoadingFromDataSource:dataSource];
    else
        [resourceLoadDelegate resource:identifier didFinishLoadingFromDataSource:dataSource];

    [[WebStandardPanels sharedStandardPanels] _didStopLoadingURL:currentURL inController:[dataSource controller]];

    [self _releaseResources];
}

- (void)handle:(WebResourceHandle *)h didFailLoadingWithError:(WebError *)result
{
    ASSERT(handle == h);
    ASSERT(!reachedTerminalState);
    
    if ([self isDownload])
        [downloadDelegate resource: identifier didFailLoadingWithError: result fromDataSource: dataSource];
    else
        [resourceLoadDelegate resource: identifier didFailLoadingWithError: result fromDataSource: dataSource];

    [[WebStandardPanels sharedStandardPanels] _didStopLoadingURL:currentURL inController:[dataSource controller]];

    [self _releaseResources];
}

- (void)cancel
{
    ASSERT (!reachedTerminalState);

    [handle cancel];
    
    [[WebStandardPanels sharedStandardPanels] _didStopLoadingURL:currentURL inController:[dataSource controller]];

    WebError *error = [[WebError alloc] initWithErrorCode:WebErrorCodeCancelled 
        inDomain:WebErrorDomainWebFoundation failingURL:[[request URL] absoluteString]];
    if (![self isDownload])
        [resourceLoadDelegate resource: identifier didFailLoadingWithError: error fromDataSource: dataSource];

    [self _releaseResources];
}

- (WebResourceHandle *)handle
{
    return handle;
}

@end
