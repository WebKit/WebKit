/*	
    WebBaseResourceHandleDelegate.m
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebBaseResourceHandleDelegate.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebError.h>

#import <WebFoundation/NSURLConnection.h>
#import <WebFoundation/NSURLConnectionPrivate.h>
#import <WebFoundation/NSURLRequest.h>
#import <WebFoundation/NSURLRequestPrivate.h>
#import <WebFoundation/NSURLResponse.h>

#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultResourceLoadDelegate.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebStandardPanelsPrivate.h>
#import <WebKit/WebViewPrivate.h>

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

    [connection release];
    connection = nil;

    [controller release];
    controller = nil;
    
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

- (BOOL)loadWithRequest:(NSURLRequest *)r
{
    ASSERT(connection == nil);
    
    r = [self connection:connection willSendRequest:r redirectResponse:nil];
    connection = [[NSURLConnection alloc] initWithRequest:r delegate:self];
    if (defersCallbacks) {
        [connection setDefersCallbacks:YES];
    }

    return YES;
}

- (void)setDefersCallbacks:(BOOL)defers
{
    defersCallbacks = defers;
    [connection setDefersCallbacks:defers];
}

- (BOOL)defersCallbacks
{
    return defersCallbacks;
}

- (void)setDataSource:(WebDataSource *)d
{
    ASSERT(d);
    ASSERT([d _controller]);
    
    [d retain];
    [dataSource release];
    dataSource = d;

    [controller release];
    controller = [[dataSource _controller] retain];
    
    [resourceLoadDelegate release];
    resourceLoadDelegate = [[controller resourceLoadDelegate] retain];
    implementations = [controller _resourceLoadDelegateImplementations];

    [downloadDelegate release];
    downloadDelegate = [[controller downloadDelegate] retain];
}

- (WebDataSource *)dataSource
{
    return dataSource;
}

- resourceLoadDelegate
{
    return resourceLoadDelegate;
}

- downloadDelegate
{
    return downloadDelegate;
}

- (NSURLRequest *)connection:(NSURLConnection *)con willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse
{
    ASSERT(con == connection);
    ASSERT(!reachedTerminalState);
    
    NSMutableURLRequest *mutableRequest = [newRequest mutableCopy];
    [mutableRequest HTTPSetUserAgent:[controller userAgentForURL:[newRequest URL]]];
    newRequest = [mutableRequest autorelease];
    
    
    if (identifier == nil) {
        // The identifier is released after the last callback, rather than in dealloc
        // to avoid potential cycles.
        if (implementations.delegateImplementsIdentifierForRequest)
            identifier = [[resourceLoadDelegate webView: controller identifierForInitialRequest:newRequest fromDataSource:dataSource] retain];
        else
            identifier = [[[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:controller identifierForInitialRequest:newRequest fromDataSource:dataSource] retain];
    }

    if (implementations.delegateImplementsWillSendRequest)
        newRequest = [resourceLoadDelegate webView:controller resource:identifier willSendRequest:newRequest redirectResponse:redirectResponse fromDataSource:dataSource];
    else
        newRequest = [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:controller resource:identifier willSendRequest:newRequest redirectResponse:redirectResponse fromDataSource:dataSource];

    // Store a copy of the request.
    [request autorelease];

    if (currentURL) {
        [[WebStandardPanels sharedStandardPanels] _didStopLoadingURL:currentURL inController:controller];
        [currentURL release];
        currentURL = nil;
    }
    
    // Client may return a nil request, indicating that the request should be aborted.
    if (newRequest){
        request = [newRequest copy];
        currentURL = [[request URL] retain];
        if (currentURL)
            [[WebStandardPanels sharedStandardPanels] _didStartLoadingURL:currentURL inController:controller];
    }
    else {
        request = nil;
    }
    
    return request;
}

- (void)connection:(NSURLConnection *)con didReceiveResponse:(NSURLResponse *)r
{
    ASSERT(con == connection);
    ASSERT(!reachedTerminalState);

    [r retain];
    [response release];
    response = r;

    [dataSource _addResponse: r];
    
    if (implementations.delegateImplementsDidReceiveResponse)
        [resourceLoadDelegate webView:controller resource:identifier didReceiveResponse:r fromDataSource:dataSource];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:controller resource:identifier didReceiveResponse:r fromDataSource:dataSource];
}

- (void)connection:(NSURLConnection *)con didReceiveData:(NSData *)data
{
    ASSERT(con == connection);
    ASSERT(!reachedTerminalState);

    if (implementations.delegateImplementsDidReceiveContentLength)
        [resourceLoadDelegate webView:controller resource:identifier didReceiveContentLength:[data length] fromDataSource:dataSource];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:controller resource:identifier didReceiveContentLength:[data length] fromDataSource:dataSource];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)con
{
    ASSERT(con == connection);
    ASSERT(!reachedTerminalState);

    if (implementations.delegateImplementsDidFinishLoadingFromDataSource)
        [resourceLoadDelegate webView:controller resource:identifier didFinishLoadingFromDataSource:dataSource];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:controller resource:identifier didFinishLoadingFromDataSource:dataSource];

    ASSERT(currentURL);
    [[WebStandardPanels sharedStandardPanels] _didStopLoadingURL:currentURL inController:controller];

    [self _releaseResources];
}

- (void)connection:(NSURLConnection *)con didFailLoadingWithError:(WebError *)result
{
    ASSERT(con == connection);
    ASSERT(!reachedTerminalState);
    
    [[controller _resourceLoadDelegateForwarder] webView:controller resource:identifier didFailLoadingWithError:result fromDataSource:dataSource];

    // currentURL may be nil if the request was aborted
    if (currentURL)
        [[WebStandardPanels sharedStandardPanels] _didStopLoadingURL:currentURL inController:controller];

    [self _releaseResources];
}

- (void)cancelWithError:(WebError *)error
{
    ASSERT(!reachedTerminalState);

    [connection cancel];
    
    // currentURL may be nil if the request was aborted
    if (currentURL)
        [[WebStandardPanels sharedStandardPanels] _didStopLoadingURL:currentURL inController:controller];

    if (error) {
        [[controller _resourceLoadDelegateForwarder] webView:controller resource:identifier didFailLoadingWithError:error fromDataSource:dataSource];
    }

    [self _releaseResources];
}

- (void)cancel
{
    if (!reachedTerminalState) {
        [self cancelWithError:[self cancelledError]];
    }
}

- (WebError *)cancelledError
{
    return [WebError errorWithCode:WebFoundationErrorCancelled
                          inDomain:WebErrorDomainWebFoundation
                        failingURL:[[request URL] absoluteString]];
}

- (void)setIdentifier: ident
{
    if (identifier != ident){
        [identifier release];
        identifier = [ident retain];
    }
}

@end
