/*	
    WebSubresourceClient.m
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebSubresourceClient.h>

#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebViewPrivate.h>

#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLRequestPrivate.h>
#import <Foundation/NSURLResponse.h>
#import <WebKit/WebAssertions.h>
#import <Foundation/NSError_NSURLExtras.h>

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
    [newRequest setHTTPReferrer:referrer];
    
    WebView *_webView = [source _webView];
    [newRequest setHTTPCookiePolicyBaseURL:[[[[_webView mainFrame] dataSource]  request] URL]];
    [newRequest setHTTPUserAgent:[_webView userAgentForURL:URL]];
    
    BOOL succeeded = [client loadWithRequest:newRequest];
    [newRequest release];
        
    if (!succeeded) {
        [source _removeSubresourceClient:client];

        [rLoader reportError];

        NSError *badURLError = [[NSError alloc] _web_initWithDomain:NSURLErrorDomain 
                                                               code:NSURLErrorBadURL
                                                         failingURL:[URL absoluteString]];
        [_webView _receivedError:badURLError fromDataSource:source];
        [badURLError release];
        client = nil;
    }
    
    return client;
}

- (void)receivedError:(NSError *)error
{
    [[dataSource _webView] _receivedError:error fromDataSource:dataSource];
}

- (NSURLRequest *)connection:(NSURLConnection *)con willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse
{
    // FIXME: We do want to tell the client about redirects for subresources.
    // But the current API doesn't give any way to tell redirects on
    // the main page from redirects on subresources.

    // FIXME: Need to make sure client sets cookie policy base URL
    // properly on redirect when we have the new redirect
    // request-adjusting API

    return [super connection:con willSendRequest:newRequest redirectResponse:redirectResponse];
}

- (void)connection:(NSURLConnection *)con didReceiveResponse:(NSURLResponse *)r
{
    ASSERT(r);
    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    [loader receivedResponse:r];
    [super connection:con didReceiveResponse:r];
    [self release];
}

- (void)connection:(NSURLConnection *)con didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    [loader addData:data];
    [super connection:con didReceiveData:data lengthReceived:lengthReceived];
    [self release];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)con
{
    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
    
    [loader finish];
    
    [dataSource _removeSubresourceClient:self];
    
    [[dataSource _webView] _finishedLoadingResourceFromDataSource:dataSource];

    [super connectionDidFinishLoading:con];

    [self release];    
}

- (void)connection:(NSURLConnection *)con didFailWithError:(NSError *)error
{
    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
    
    [loader reportError];
    [dataSource _removeSubresourceClient:self];
    [self receivedError:error];
    [super connection:con didFailWithError:error];

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
