/*	
    WebSubresourceClient.m
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebSubresourceClient.h>

#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebViewPrivate.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebError.h>
#import <WebFoundation/WebResourceDelegate.h>
#import <WebFoundation/NSURLRequest.h>
#import <WebFoundation/NSURLRequestPrivate.h>

#import <WebFoundation/WebResponse.h>

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
    WebSubresourceClient *client = [[[self alloc] initWithLoader:rLoader dataSource:source] autorelease];
    
    [source _addSubresourceClient:client];

    NSURLRequest *newRequest = [[NSURLRequest alloc] initWithURL:URL];
    [newRequest setCachePolicy:[[source request] cachePolicy]];
    [newRequest HTTPSetReferrer:referrer];
    
    WebView *_controller = [source _controller];
    [newRequest HTTPSetCookiePolicyBaseURL:[[[[_controller mainFrame] dataSource]  request] URL]];
    [newRequest HTTPSetUserAgent:[_controller userAgentForURL:URL]];
    
    BOOL succeeded = [client loadWithRequest:newRequest];
    [newRequest release];
        
    if (!succeeded) {
        [source _removeSubresourceClient:client];

        [rLoader reportError];

        WebError *badURLError = [[WebError alloc] initWithErrorCode:WebFoundationErrorBadURL
                                                           inDomain:WebErrorDomainWebFoundation
                                                         failingURL:[URL absoluteString]];
        [_controller _receivedError:badURLError fromDataSource:source];
        [badURLError release];
        client = nil;
    }
    
    return client;
}

- (void)receivedError:(WebError *)error
{
    [[dataSource _controller] _receivedError:error fromDataSource:dataSource];
}

-(NSURLRequest *)resource:(WebResource *)h willSendRequest:(NSURLRequest *)newRequest
{
    // FIXME: We do want to tell the client about redirects for subresources.
    // But the current API doesn't give any way to tell redirects on
    // the main page from redirects on subresources.

    // FIXME: Need to make sure client sets cookie policy base URL
    // properly on redirect when we have the new redirect
    // request-adjusting API

    return [super resource: h willSendRequest: newRequest];
}

-(void)resource:(WebResource *)h didReceiveResponse:(WebResponse *)r
{
    ASSERT(r);
    [loader receivedResponse:r];
    [super resource:h didReceiveResponse:r];
}

- (void)resource:(WebResource *)h didReceiveData:(NSData *)data
{
    [loader addData:data];
    [super resource:h didReceiveData:data];
}

- (void)resourceDidFinishLoading:(WebResource *)h
{
    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
    
    [loader finish];
    
    [dataSource _removeSubresourceClient:self];
    
    [[dataSource _controller] _finishedLoadingResourceFromDataSource:dataSource];
    
    [self release];
    
    [super resourceDidFinishLoading:h];
}

- (void)resource:(WebResource *)h didFailLoadingWithError:(WebError *)error
{
    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
    
    [loader reportError];
    [dataSource _removeSubresourceClient:self];
    [self receivedError:error];
    [super resource:h didFailLoadingWithError:error];

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
