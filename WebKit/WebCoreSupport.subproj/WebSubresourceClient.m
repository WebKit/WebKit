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
#import <WebFoundation/NSURLConnection.h>
#import <WebFoundation/NSURLRequest.h>
#import <WebFoundation/NSURLRequestPrivate.h>

#import <WebFoundation/NSURLResponse.h>

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

    NSMutableURLRequest *newRequest = [[NSMutableURLRequest alloc] initWithURL:URL];
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

- (NSURLRequest *)connection:(NSURLConnection *)connection willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse
{
    // FIXME: We do want to tell the client about redirects for subresources.
    // But the current API doesn't give any way to tell redirects on
    // the main page from redirects on subresources.

    // FIXME: Need to make sure client sets cookie policy base URL
    // properly on redirect when we have the new redirect
    // request-adjusting API

    return [super connection:connection willSendRequest:newRequest redirectResponse:redirectResponse];
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)r
{
    ASSERT(r);
    [loader receivedResponse:r];
    [super connection:connection didReceiveResponse:r];
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
    [loader addData:data];
    [super connection:connection didReceiveData:data];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
    
    [loader finish];
    
    [dataSource _removeSubresourceClient:self];
    
    [[dataSource _controller] _finishedLoadingResourceFromDataSource:dataSource];
    
    [self release];
    
    [super connectionDidFinishLoading:connection];
}

- (void)connection:(NSURLConnection *)connection didFailLoadingWithError:(WebError *)error
{
    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
    
    [loader reportError];
    [dataSource _removeSubresourceClient:self];
    [self receivedError:error];
    [super connection:connection didFailLoadingWithError:error];

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
