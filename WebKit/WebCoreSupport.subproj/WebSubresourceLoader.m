/*	
    WebSubresourceClient.m
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebBridge.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebSubresourceClient.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebError.h>
#import <WebFoundation/WebResourceHandle.h>
#import <WebFoundation/WebResourceHandlePrivate.h>
#import <WebFoundation/WebResourceRequest.h>
#import <WebFoundation/WebHTTPResourceRequest.h>
#import <WebFoundation/WebResourceResponse.h>

#import <WebCore/WebCoreResourceLoader.h>


@implementation WebSubresourceClient

- initWithLoader:(id <WebCoreResourceLoader>)l dataSource:(WebDataSource *)s
{
    [super init];
    
    loader = [l retain];
    dataSource = [s retain];

    resourceProgressDelegate = [[[dataSource controller] resourceLoadDelegate] retain];
    
    return self;
}

- (void)dealloc
{
    ASSERT(currentURL == nil);
    
    [loader release];
    [dataSource release];
    [handle release];
    [request release];
    [response release];
    [resourceProgressDelegate release];
    
    [super dealloc];
}

- (void)didStartLoadingWithURL:(NSURL *)URL
{
    ASSERT(currentURL == nil);
    currentURL = [URL retain];
    [[dataSource controller] _didStartLoading:currentURL];
}

- (void)didStopLoading
{
    ASSERT(currentURL != nil);
    [[dataSource controller] _didStopLoading:currentURL];
    [currentURL release];
    currentURL = nil;
}


+ (WebSubresourceClient *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
    withURL:(NSURL *)URL referrer:(NSString *)referrer forDataSource:(WebDataSource *)source
{
    WebSubresourceClient *client = [[self alloc] initWithLoader:rLoader dataSource:source];
    WebResourceRequest *newRequest = [[WebResourceRequest alloc] initWithURL:URL];
    [newRequest setRequestCachePolicy:[[source request] requestCachePolicy]];
    [newRequest setResponseCachePolicy:[[source request] responseCachePolicy]];
    [newRequest setReferrer:referrer];
    [newRequest setCookiePolicyBaseURL:[[[[source controller] mainFrame] dataSource] URL]];
    [newRequest setUserAgent:[[source controller] userAgentForURL:URL]];
    
    if (![WebResourceHandle canInitWithRequest:newRequest]) {
        [newRequest release];
        [rLoader cancel];

        WebError *badURLError = [[WebError alloc] initWithErrorCode:WebErrorCodeBadURLError
                                                           inDomain:WebErrorDomainWebFoundation
                                                         failingURL:[URL absoluteString]];
        [[source controller] _receivedError:badURLError forResourceHandle:nil
            fromDataSource:source];
        [badURLError release];
        return nil;
    }
    
    WebResourceHandle *h = [[WebResourceHandle alloc] initWithRequest:newRequest];
    client->handle = h;
    [source _addSubresourceClient:client];
    [client didStartLoadingWithURL:[newRequest URL]];
    [h loadWithDelegate:client];
    [newRequest release];
        
    return [client autorelease];
}

- (void)receivedError:(WebError *)error
{
    [[dataSource controller] _receivedError:error forResourceHandle:handle
        fromDataSource:dataSource];
}

-(WebResourceRequest *)handle:(WebResourceHandle *)h willSendRequest:(WebResourceRequest *)newRequest
{
    ASSERT(handle == h);

    // FIXME: We do want to tell the client about redirects for subresources.
    // But the current API doesn't give any way to tell redirects on
    // the main page from redirects on subresources.

    // FIXME: Need to make sure client sets cookie policy base URL
    // properly on redirect when we have the new redirect
    // request-adjusting API

    WebController *controller = [dataSource controller];
    NSURL *URL = [request URL];

    [newRequest setUserAgent:[controller userAgentForURL:URL]];

    // Not the first send, so reload.
    if (request) {
        [self didStopLoading];
        [self didStartLoadingWithURL:URL];
    }

    // Let the resourceProgressDelegate get a crack at modifying the request.
    if (resourceProgressDelegate){
        if (identifier == nil){
            // The identifier is released after the last callback, rather than in dealloc
            // to avoid potential cycles.
            identifier = [[resourceProgressDelegate identifierForInitialRequest: newRequest fromDataSource: dataSource] retain];
        }
        newRequest = [resourceProgressDelegate resource: identifier willSendRequest: newRequest fromDataSource: dataSource];
    }
        
    ASSERT (newRequest != nil); 

    [newRequest retain];
    [request release];
    request = newRequest;

    return newRequest;
}

-(void)handle:(WebResourceHandle *)h didReceiveResponse:(WebResourceResponse *)r
{
    ASSERT(handle == h);

    [r retain];
    [response release];
    response = r;
    [resourceProgressDelegate resource: identifier didReceiveResponse: r fromDataSource: dataSource];

    [loader receivedResponse:r];
}

- (void)handle:(WebResourceHandle *)h didReceiveData:(NSData *)data
{
    ASSERT(handle == h);

    [resourceProgressDelegate resource: identifier didReceiveContentLength: [data length] 
        fromDataSource: dataSource];

    [loader addData:data];
}

- (void)handleDidFinishLoading:(WebResourceHandle *)h
{
    ASSERT(handle == h);

    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
    
    [loader finish];
    
    [dataSource _removeSubresourceClient:self];
    
    WebError *nonTerminalError = [response error];
    if (nonTerminalError) {
        [self receivedError:nonTerminalError];
    }
    
    [resourceProgressDelegate resource:identifier didFinishLoadingFromDataSource:dataSource];

    [[dataSource controller] _finsishedLoadingResourceFromDataSource:dataSource];
    
    [self didStopLoading];

    [handle release];
    handle = nil;
    
    [identifier release];
    identifier = nil;
    
    [self release];
}

- (void)handle:(WebResourceHandle *)h didFailLoadingWithError:(WebError *)error
{
    ASSERT(handle == h);
    
    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
    
    [loader cancel];
    
    [dataSource _removeSubresourceClient:self];

    [resourceProgressDelegate resource: identifier didFailLoadingWithError: error fromDataSource: dataSource];
    
    [self receivedError:error];

    [self didStopLoading];

    [handle release];
    handle = nil;

    [identifier release];
    identifier = nil;
        
    [self release];
}

- (void)cancel
{
    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
    
    [handle cancel];
    
    [loader cancel];
    
    [dataSource _removeSubresourceClient:self];
        
    WebError *error = [[WebError alloc] initWithErrorCode:WebErrorCodeCancelled 
        inDomain:WebErrorDomainWebFoundation failingURL:[[request URL] absoluteString]];
    [self receivedError:error];
    [error release];

    [self didStopLoading];

    [handle release];
    handle = nil;
    
    [self release];
}

- (WebResourceHandle *)handle
{
    return handle;
}

@end
