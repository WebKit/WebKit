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
#import <WebKit/WebKitDebug.h>

@implementation WebSubresourceClient

- initWithLoader:(id <WebCoreResourceLoader>)l dataSource:(WebDataSource *)s
{
    [super init];
    
    loader = [l retain];
    dataSource = [s retain];
    
    return self;
}

- (void)didStartLoadingWithURL:(NSURL *)URL
{
    WEBKIT_ASSERT(currentURL == nil);
    currentURL = [URL retain];
    [[dataSource controller] _didStartLoading:currentURL];
}

- (void)didStopLoading
{
    WEBKIT_ASSERT(currentURL != nil);
    [[dataSource controller] _didStopLoading:currentURL];
    [currentURL release];
    currentURL = nil;
}

- (void)dealloc
{
    WEBKIT_ASSERT(currentURL == nil);
    
    [loader release];
    [dataSource release];
    
    [super dealloc];
}

+ (WebResourceHandle *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
    withURL:(NSURL *)URL dataSource:(WebDataSource *)source
{
    WebSubresourceClient *client = [[self alloc] initWithLoader:rLoader dataSource:source];
    WebResourceRequest *request = [[WebResourceRequest alloc] initWithClient:client URL:URL attributes:nil flags:[source flags]];
    WebResourceHandle *handle = [[[WebResourceHandle alloc] initWithRequest:request] autorelease];
    [client release];
    [request release];

    if (handle == nil) {
        [rLoader cancel];

        WebError *badURLError = [[WebError alloc] initWithErrorCode:WebResultBadURLError
                                                           inDomain:WebErrorDomainWebFoundation
                                                         failingURL:[URL absoluteString]];
        [[source controller] _receivedError:badURLError forResourceHandle:nil
            partialProgress:nil fromDataSource:source];
        [badURLError release];
    } else {
        [source _addResourceHandle:handle];
        [handle loadInBackground];
    }
        
    return handle;
}

- (void)receivedProgressWithHandle:(WebResourceHandle *)handle complete:(BOOL)isComplete
{
    [[dataSource controller] _receivedProgress:[WebLoadProgress progressWithResourceHandle:handle]
        forResourceHandle:handle fromDataSource:dataSource complete:isComplete];
}

- (void)receivedError:(WebError *)error forHandle:(WebResourceHandle *)handle
{
    [[dataSource controller] _receivedError:error forResourceHandle:handle
        partialProgress:[WebLoadProgress progressWithResourceHandle:handle] fromDataSource:dataSource];
}

- (NSString *)handleWillUseUserAgent:(WebResourceHandle *)handle forURL:(NSURL *)URL
{
    return [[dataSource controller] userAgentForURL:URL];
}

- (void)handleDidBeginLoading:(WebResourceHandle *)handle
{
    [self didStartLoadingWithURL:[handle URL]];
    [self receivedProgressWithHandle:handle complete: NO];
}

- (void)handleDidReceiveData:(WebResourceHandle *)handle data:(NSData *)data
{
    WEBKIT_ASSERT([currentURL isEqual:[handle URL]]);

    [self receivedProgressWithHandle:handle complete: NO];
    [loader addData:data];
}

- (void)handleDidCancelLoading:(WebResourceHandle *)handle
{
    WebError *error;
    
    [loader cancel];
    
    [dataSource _removeResourceHandle:handle];
        
    error = [[WebError alloc] initWithErrorCode:WebResultCancelled 
        inDomain:WebErrorDomainWebFoundation failingURL:[[dataSource originalURL] absoluteString]];
    [self receivedError:error forHandle:handle];
    [error release];

    [self didStopLoading];
}

- (void)handleDidFinishLoading:(WebResourceHandle *)handle
{    
    WEBKIT_ASSERT([currentURL isEqual:[handle URL]]);
    WEBKIT_ASSERT([[handle response] statusCode] == WebResourceHandleStatusLoadComplete);

    [loader finish];
    
    [dataSource _removeResourceHandle:handle];
    
    WebError *nonTerminalError = [handle error];
    if (nonTerminalError) {
        [self receivedError:nonTerminalError forHandle:handle];
    }
    
    [self receivedProgressWithHandle:handle complete:YES];
    
    [self didStopLoading];
}

- (void)handleDidFailLoading:(WebResourceHandle *)handle withError:(WebError *)error
{
    WEBKIT_ASSERT([currentURL isEqual:[handle URL]]);

    [loader cancel];
    
    [dataSource _removeResourceHandle:handle];
    
    [self receivedError:error forHandle:handle];

    [self didStopLoading];
}

- (void)handleDidRedirect:(WebResourceHandle *)handle toURL:(NSURL *)URL
{
    WEBKIT_ASSERT(currentURL != nil);
    WEBKIT_ASSERT([URL isEqual:[handle URL]]);

    // FIXME: We do want to tell the client about redirects.
    // But the current API doesn't give any way to tell redirects on
    // the main page from redirects on subresources, so for now we are
    // just disabling this. Before, we had code that tried to send the
    // redirect, but sent it to the wrong object.
    //[[dataSource _locationChangeHandler] serverRedirectTo:toURL forDataSource:dataSource];
    
    [self didStopLoading];
    [self didStartLoadingWithURL:URL];
}

@end
