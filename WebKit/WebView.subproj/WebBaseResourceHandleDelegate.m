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
#import <WebKit/WebKitErrors.h>
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

- (BOOL)loadWithRequest:(WebResourceRequest *)r
{
    ASSERT(handle == nil);
    
    handle = [[WebResourceHandle alloc] initWithRequest:r];
    if (!handle) {
        return NO;
    }
    if (defersCallbacks) {
        [handle _setDefersCallbacks:YES];
    }
    [handle loadWithDelegate:self];
    return YES;
}

- (void)setDefersCallbacks:(BOOL)defers
{
    defersCallbacks = defers;
    [handle _setDefersCallbacks:defers];
}

- (void)setDataSource:(WebDataSource *)d
{
    [d retain];
    [dataSource release];
    dataSource = d;
    
    [resourceLoadDelegate release];
    resourceLoadDelegate = [[[dataSource controller] resourceLoadDelegate] retain];

    [downloadDelegate release];
    downloadDelegate = [[[dataSource controller] downloadDelegate] retain];
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

- (void)setIsDownload:(BOOL)f
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

    if (identifier == nil) {
        // The identifier is released after the last callback, rather than in dealloc
        // to avoid potential cycles.
        identifier = [[resourceLoadDelegate identifierForInitialRequest:newRequest fromDataSource:dataSource] retain];
    }

    if (resourceLoadDelegate) {
        newRequest = [resourceLoadDelegate resource:identifier willSendRequest:newRequest fromDataSource:dataSource];
    }

    // Store a copy of the request.
    [request autorelease];
    request = [newRequest copy];

    if (currentURL) {
        [[WebStandardPanels sharedStandardPanels] _didStopLoadingURL:currentURL inController:[dataSource controller]];
    }    
    [currentURL release];
    currentURL = [[request URL] retain];
    [[WebStandardPanels sharedStandardPanels] _didStartLoadingURL:currentURL inController:[dataSource controller]];

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
        [downloadDelegate resource:identifier didReceiveResponse:r fromDataSource:dataSource];
    else
        [resourceLoadDelegate resource:identifier didReceiveResponse:r fromDataSource:dataSource];
}

- (void)handle:(WebResourceHandle *)h didReceiveData:(NSData *)data
{
    ASSERT(handle == h);
    ASSERT(!reachedTerminalState);

    if ([self isDownload])
        [downloadDelegate resource:identifier didReceiveContentLength:[data length] fromDataSource:dataSource];
    else
        [resourceLoadDelegate resource:identifier didReceiveContentLength:[data length] fromDataSource:dataSource];
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
        [downloadDelegate resource:identifier didFailLoadingWithError:result fromDataSource:dataSource];
    else
        [resourceLoadDelegate resource:identifier didFailLoadingWithError:result fromDataSource:dataSource];

    [[WebStandardPanels sharedStandardPanels] _didStopLoadingURL:currentURL inController:[dataSource controller]];

    [self _releaseResources];
}

- (void)_cancelWithError:(WebError *)error
{
    ASSERT(!reachedTerminalState);

    [handle cancel];
    
    [[WebStandardPanels sharedStandardPanels] _didStopLoadingURL:currentURL inController:[dataSource controller]];

    if (error) {
        [resourceLoadDelegate resource:identifier didFailLoadingWithError:error fromDataSource:dataSource];
    }

    [self _releaseResources];
}

- (void)cancel
{
    [self _cancelWithError:[self isDownload] ? nil : [self cancelledError]];
}

- (void)cancelQuietly
{
    [self _cancelWithError:nil];
}

- (WebError *)cancelledError
{
    return [WebError errorWithCode:WebErrorCodeCancelled 
        inDomain:WebErrorDomainWebFoundation failingURL:[[request URL] absoluteString]];
}

- (void)notifyDelegatesOfInterruptionByPolicyChange
{
    WebError *error = [WebError errorWithCode:WebErrorResourceLoadInterruptedByPolicyChange inDomain:WebErrorDomainWebKit failingURL:nil];
    [[self resourceLoadDelegate] resource:identifier didFailLoadingWithError:error fromDataSource:dataSource];
}

@end
