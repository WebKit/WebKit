//
//  WebSubresourceClient.m
//  WebKit
//
//  Created by Darin Adler on Sat Jun 15 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebSubresourceClient.h>

#import <WebFoundation/WebError.h>
#import <WebFoundation/WebResourceHandle.h>
#import <WebFoundation/WebResourceRequest.h>
#import <WebFoundation/WebResourceResponse.h>

#import <WebCore/WebCoreResourceLoader.h>

#import <WebKit/WebLoadProgress.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebFoundation/WebAssertions.h>

@implementation WebSubresourceClient

- initWithLoader:(id <WebCoreResourceLoader>)l dataSource:(WebDataSource *)s
{
    [super init];
    
    loader = [l retain];
    dataSource = [s retain];
    
    return self;
}

- (void)dealloc
{
    ASSERT(currentURL == nil);
    
    [loader release];
    [dataSource release];
    [handle release];
    
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

- (void)receivedProgressWithComplete:(BOOL)isComplete
{
    [[dataSource controller] _receivedProgress:[WebLoadProgress progressWithResourceHandle:handle]
        forResourceHandle:handle fromDataSource:dataSource complete:isComplete];
}

+ (WebSubresourceClient *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
    withURL:(NSURL *)URL attributes:(NSDictionary *)attributes forDataSource:(WebDataSource *)source
{
    WebSubresourceClient *client = [[self alloc] initWithLoader:rLoader dataSource:source];
    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:URL attributes:attributes flags:[source flags]];
    WebResourceHandle *h = [[[WebResourceHandle alloc] initWithRequest:request client:client] autorelease];
    [request release];
    
    if (h == nil) {
        [rLoader cancel];

        WebError *badURLError = [[WebError alloc] initWithErrorCode:WebResultBadURLError
                                                           inDomain:WebErrorDomainWebFoundation
                                                         failingURL:[URL absoluteString]];
        [[source controller] _receivedError:badURLError forResourceHandle:nil
            partialProgress:nil fromDataSource:source];
        [badURLError release];
        return nil;
    }
    
    client->handle = [h retain];
    [source _addSubresourceClient:client];
    [client didStartLoadingWithURL:[h URL]];
    [client receivedProgressWithComplete:NO];
    [h loadInBackground];
        
    return [client autorelease];
}

- (void)receivedError:(WebError *)error
{
    [[dataSource controller] _receivedError:error forResourceHandle:handle
        partialProgress:[WebLoadProgress progressWithResourceHandle:handle] fromDataSource:dataSource];
}

- (NSString *)handleWillUseUserAgent:(WebResourceHandle *)h forURL:(NSURL *)URL
{
    return [[dataSource controller] userAgentForURL:URL];
}

- (void)handleDidReceiveData:(WebResourceHandle *)h data:(NSData *)data
{
    ASSERT(handle == h);
    ASSERT([currentURL isEqual:[handle URL]]);

    [self receivedProgressWithComplete:NO];
    [loader addData:data];
}

- (void)handleDidFinishLoading:(WebResourceHandle *)h
{
    ASSERT(handle == h);
    ASSERT([currentURL isEqual:[handle URL]]);
    ASSERT([[handle response] statusCode] == WebResourceHandleStatusLoadComplete);

    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
    
    [loader finish];
    
    [dataSource _removeSubresourceClient:self];
    
    WebError *nonTerminalError = [[handle response] error];
    if (nonTerminalError) {
        [self receivedError:nonTerminalError];
    }
    
    [self receivedProgressWithComplete:YES];
    
    [self didStopLoading];

    [handle release];
    handle = nil;
    
    [self release];
}

- (void)handleDidFailLoading:(WebResourceHandle *)h withError:(WebError *)error
{
    ASSERT(handle == h);
    ASSERT([currentURL isEqual:[handle URL]]);
    
    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
    
    [loader cancel];
    
    [dataSource _removeSubresourceClient:self];
    
    [self receivedError:error];

    [self didStopLoading];

    [handle release];
    handle = nil;
    
    [self release];
}

- (void)handleDidRedirect:(WebResourceHandle *)h toURL:(NSURL *)URL
{
    ASSERT(handle == h);
    ASSERT(currentURL != nil);
    ASSERT([URL isEqual:[handle URL]]);

    // FIXME: We do want to tell the client about redirects.
    // But the current API doesn't give any way to tell redirects on
    // the main page from redirects on subresources, so for now we are
    // just disabling this. Before, we had code that tried to send the
    // redirect, but sent it to the wrong object.
    //[[dataSource _locationChangeHandler] serverRedirectTo:toURL forDataSource:dataSource];
    
    [self didStopLoading];
    [self didStartLoadingWithURL:URL];
}

- (void)cancel
{
    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
    
    [handle cancelLoadInBackground];
    
    [loader cancel];
    
    [dataSource _removeSubresourceClient:self];
        
    WebError *error = [[WebError alloc] initWithErrorCode:WebResultCancelled 
        inDomain:WebErrorDomainWebFoundation failingURL:[[dataSource originalURL] absoluteString]];
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
