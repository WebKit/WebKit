//
//  IFResourceURLHandleClient.m
//  WebKit
//
//  Created by Darin Adler on Sat Jun 15 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/IFResourceURLHandleClient.h>

#import <WebFoundation/IFError.h>
#import <WebFoundation/IFURLHandle.h>

#import <WebCoreResourceLoader.h>

#import <WebKit/IFLoadProgress.h>
#import <WebKit/IFWebControllerPrivate.h>
#import <WebKit/IFWebCoreBridge.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/WebKitDebug.h>

@implementation IFResourceURLHandleClient

- initWithLoader:(id <WebCoreResourceLoader>)l dataSource:(IFWebDataSource *)s
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

+ (IFURLHandle *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
    withURL:(NSURL *)URL dataSource:(IFWebDataSource *)source
{
    IFURLHandle *handle;
    IFResourceURLHandleClient *client;
    
    handle = [[[IFURLHandle alloc] initWithURL:URL attributes:nil flags:0] autorelease];
    if (handle == nil) {
        [rLoader cancel];

        IFError *badURLError = [IFError errorWithCode:IFURLHandleResultBadURLError
                                             inDomain:IFErrorCodeDomainWebFoundation
                                           failingURL:URL
                                           isTerminal:YES];        
        [[source controller] _receivedError:badURLError forResourceHandle:nil
            partialProgress:nil fromDataSource:source];
        
        return nil;
    } else {
        [source _addURLHandle:handle];
        
        client = [[self alloc] initWithLoader:rLoader dataSource:source];
        [handle addClient:client];
        [client release];
        
        [handle loadInBackground];
        
        return handle;
    }
}

- (void)receivedProgressWithHandle:(IFURLHandle *)handle complete:(BOOL)isComplete
{
    [[dataSource controller] _receivedProgress:[IFLoadProgress progressWithURLHandle:handle]
        forResourceHandle:handle fromDataSource:dataSource complete:isComplete];
}

- (void)IFURLHandleResourceDidBeginLoading:(IFURLHandle *)handle
{
    [self didStartLoadingWithURL:[handle url]];
    [self receivedProgressWithHandle:handle complete: NO];
}

- (void)IFURLHandle:(IFURLHandle *)handle resourceDataDidBecomeAvailable:(NSData *)data
{
    WEBKIT_ASSERT([currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);

    [self receivedProgressWithHandle:handle complete: NO];
    [loader addData:data];
}

- (void)IFURLHandleResourceDidCancelLoading:(IFURLHandle *)handle
{
    [loader cancel];
    
    [dataSource _removeURLHandle:handle];
        
    [[dataSource controller] _receivedProgress:[IFLoadProgress progress]
        forResourceHandle:handle fromDataSource:dataSource complete: YES];

    [self didStopLoading];
}

- (void)IFURLHandleResourceDidFinishLoading:(IFURLHandle *)handle data:(NSData *)data
{    
    WEBKIT_ASSERT([currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);
    WEBKIT_ASSERT([handle statusCode] == IFURLHandleStatusLoadComplete);
    WEBKIT_ASSERT((int)[data length] == [handle contentLengthReceived]);

    [loader finish];
    
    [dataSource _removeURLHandle:handle];
    
    IFError *nonTerminalError = [handle error];
    if (nonTerminalError) {
        [[dataSource controller] _receivedError:nonTerminalError forResourceHandle:handle
            partialProgress:[IFLoadProgress progressWithURLHandle:handle] fromDataSource:dataSource];
    }
    
    [self receivedProgressWithHandle:handle complete:YES];
    
    [self didStopLoading];
}

- (void)IFURLHandle:(IFURLHandle *)handle resourceDidFailLoadingWithResult:(IFError *)error
{
    WEBKIT_ASSERT([currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);

    [loader cancel];
    
    [dataSource _removeURLHandle:handle];
    
    [[dataSource controller] _receivedError:error forResourceHandle:handle
        partialProgress:[IFLoadProgress progressWithURLHandle:handle] fromDataSource:dataSource];

    [self didStopLoading];
}

- (void)IFURLHandle:(IFURLHandle *)handle didRedirectToURL:(NSURL *)URL
{
    WEBKIT_ASSERT(currentURL != nil);
    WEBKIT_ASSERT([URL isEqual:[handle redirectedURL]]);

    [[dataSource _bridge] setURL:URL];

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
