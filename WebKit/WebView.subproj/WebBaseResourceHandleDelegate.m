/*	
    WebBaseResourceHandleDelegate.m
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebBaseResourceHandleDelegate.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebError.h>
#import <WebFoundation/WebHTTPRequest.h>
#import <WebFoundation/WebResource.h>
#import <WebFoundation/WebRequest.h>
#import <WebFoundation/WebResponse.h>

#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultResourceLoadDelegate.h>
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

    [resource release];
    resource = nil;

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

- (void)startLoading:(WebRequest *)r
{
    [resource loadWithDelegate:self];
}

- (BOOL)loadWithRequest:(WebRequest *)r
{
    ASSERT(resource == nil);
    
    resource = [[WebResource alloc] initWithRequest:r];
    if (!resource) {
        return NO;
    }
    if (defersCallbacks) {
        [resource setDefersCallbacks:YES];
    }

    [self startLoading:r];

    return YES;
}

- (void)setDefersCallbacks:(BOOL)defers
{
    defersCallbacks = defers;
    [resource setDefersCallbacks:defers];
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

-(WebRequest *)resource:(WebResource *)h willSendRequest:(WebRequest *)newRequest
{
    ASSERT(resource == h);
    ASSERT(!reachedTerminalState);
    
    [newRequest setUserAgent:[controller userAgentForURL:[newRequest URL]]];

    if (identifier == nil) {
        // The identifier is released after the last callback, rather than in dealloc
        // to avoid potential cycles.
        if ([resourceLoadDelegate respondsToSelector: @selector(controller:identifierForInitialRequest:fromDataSource:)])
            identifier = [[resourceLoadDelegate controller: controller identifierForInitialRequest:newRequest fromDataSource:dataSource] retain];
        else
            identifier = [[[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] controller:controller identifierForInitialRequest:newRequest fromDataSource:dataSource] retain];
    }

    if (resourceLoadDelegate) {
        if ([resourceLoadDelegate respondsToSelector: @selector(controller:resource:willSendRequest:fromDataSource:)])
            newRequest = [resourceLoadDelegate controller:controller resource:identifier willSendRequest:newRequest fromDataSource:dataSource];
        else
            newRequest = [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] controller:controller resource:identifier willSendRequest:newRequest fromDataSource:dataSource];
    }

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

-(void)resource:(WebResource *)h didReceiveResponse:(WebResponse *)r
{
    ASSERT(resource == h);
    ASSERT(!reachedTerminalState);

    [r retain];
    [response release];
    response = r;

    [dataSource _addResponse: r];
    [[controller _resourceLoadDelegateForwarder] controller:controller resource:identifier didReceiveResponse:r fromDataSource:dataSource];
}

- (void)resource:(WebResource *)h didReceiveData:(NSData *)data
{
    ASSERT(resource == h);
    ASSERT(!reachedTerminalState);

    [[controller _resourceLoadDelegateForwarder] controller:controller resource:identifier didReceiveContentLength:[data length] fromDataSource:dataSource];
}

- (void)resourceDidFinishLoading:(WebResource *)h
{
    ASSERT(resource == h);
    ASSERT(!reachedTerminalState);

    [[controller _resourceLoadDelegateForwarder] controller:controller resource:identifier didFinishLoadingFromDataSource:dataSource];

    ASSERT(currentURL);
    [[WebStandardPanels sharedStandardPanels] _didStopLoadingURL:currentURL inController:controller];

    [self _releaseResources];
}

- (void)resource:(WebResource *)h didFailLoadingWithError:(WebError *)result
{
    ASSERT(resource == h);
    ASSERT(!reachedTerminalState);
    
    [[controller _resourceLoadDelegateForwarder] controller:controller resource:identifier didFailLoadingWithError:result fromDataSource:dataSource];

    // currentURL may be nil if the request was aborted
    if (currentURL)
        [[WebStandardPanels sharedStandardPanels] _didStopLoadingURL:currentURL inController:controller];

    [self _releaseResources];
}

- (void)cancelWithError:(WebError *)error
{
    ASSERT(!reachedTerminalState);

    [resource cancel];
    
    // currentURL may be nil if the request was aborted
    if (currentURL)
        [[WebStandardPanels sharedStandardPanels] _didStopLoadingURL:currentURL inController:controller];

    if (error) {
        [[controller _resourceLoadDelegateForwarder] controller:controller resource:identifier didFailLoadingWithError:error fromDataSource:dataSource];
    }

    [self _releaseResources];
}

- (void)cancel
{
    [self cancelWithError:[self cancelledError]];
}

- (void)cancelQuietly
{
    [self cancelWithError:nil];
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
