/*	
    WebSubresourceClient.m
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebSubresourceClient.h>

#import <WebKit/WebBridge.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebResourceLoadDelegate.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebError.h>
#import <WebFoundation/WebResourceHandle.h>
#import <WebFoundation/WebResourceRequest.h>
#import <WebFoundation/WebHTTPResourceRequest.h>
#import <WebFoundation/WebResourceResponse.h>

#import <WebCore/WebCoreResourceLoader.h>

@implementation WebSubresourceClient

- initWithLoader:(id <WebCoreResourceLoader>)l dataSource:(WebDataSource *)s
{
    [super init];
    
    loader = [l retain];

    [self setDataSource: s];
    
    return self;
}

- (void)dealloc
{
    [loader release];
    [super dealloc];
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
        [rLoader reportError];

        WebError *badURLError = [[WebError alloc] initWithErrorCode:WebErrorCodeBadURLError
                                                           inDomain:WebErrorDomainWebFoundation
                                                         failingURL:[URL absoluteString]];
        [[source controller] _receivedError:badURLError fromDataSource:source];
        [badURLError release];
        return nil;
    }
    
    [source _addSubresourceClient:client];
    [client loadWithRequest:newRequest];
    [newRequest release];
        
    return [client autorelease];
}

- (void)receivedError:(WebError *)error
{
    [[dataSource controller] _receivedError:error fromDataSource:dataSource];
}

-(WebResourceRequest *)handle:(WebResourceHandle *)h willSendRequest:(WebResourceRequest *)newRequest
{
    // FIXME: We do want to tell the client about redirects for subresources.
    // But the current API doesn't give any way to tell redirects on
    // the main page from redirects on subresources.

    // FIXME: Need to make sure client sets cookie policy base URL
    // properly on redirect when we have the new redirect
    // request-adjusting API

    return [super handle: h willSendRequest: newRequest];
}

-(void)handle:(WebResourceHandle *)h didReceiveResponse:(WebResourceResponse *)r
{
    ASSERT(r);
    [loader receivedResponse:r];
    [super handle:h didReceiveResponse:r];
}

- (void)handle:(WebResourceHandle *)h didReceiveData:(NSData *)data
{
    [loader addData:data];
    [super handle:h didReceiveData:data];
}

- (void)handleDidFinishLoading:(WebResourceHandle *)h
{
    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
    
    [loader finish];
    
    [dataSource _removeSubresourceClient:self];
    
    [[dataSource controller] _finishedLoadingResourceFromDataSource:dataSource];
    
    [self release];
    
    [super handleDidFinishLoading:h];
}

- (void)handle:(WebResourceHandle *)h didFailLoadingWithError:(WebError *)error
{
    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
    
    [loader reportError];
    [dataSource _removeSubresourceClient:self];
    [self receivedError:error];
    [super handle:h didFailLoadingWithError:error];

    [self release];
}

- (void)cancel
{
    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
        
    [loader cancel];
    [dataSource _removeSubresourceClient:self];
    [self receivedError:[self cancelledError]];
    [super cancel];

    [self release];
}

@end
