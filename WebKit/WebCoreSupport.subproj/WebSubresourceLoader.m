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

- (void)dealloc
{
#ifdef WEBFOUNDATION_LOAD_MESSAGES_FIXED
    WEBKIT_ASSERT(currentURL == nil);
#else
    [currentURL release];
#endif
    
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
            inDomain:IFErrorCodeDomainWebFoundation isTerminal:YES];        
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

- (void)receivedProgressWithHandle:(IFURLHandle *)handle complete: (BOOL)isComplete
{
    [[dataSource controller] _receivedProgress:[IFLoadProgress progressWithURLHandle:handle]
        forResourceHandle:handle fromDataSource:dataSource complete:isComplete];
}

- (void)IFURLHandleResourceDidBeginLoading:(IFURLHandle *)handle
{
    WEBKIT_ASSERT(currentURL == nil);

    currentURL = [[handle url] retain];
    [[dataSource controller] _didStartLoading:[handle url]];
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
    // FIXME Radar 2954901: Replaced the assertion below with a more lenient one,
    // since cancel messages are sometimes sent before begin.
#ifdef WEBFOUNDATION_LOAD_MESSAGES_FIXED    
    WEBKIT_ASSERT([currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);
#else
    WEBKIT_ASSERT(currentURL == nil || [currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);
#endif    

    [dataSource _removeURLHandle:handle];
        
    [loader cancel];
    
    [[dataSource controller] _receivedProgress:[IFLoadProgress progress]
        forResourceHandle:handle fromDataSource:dataSource complete: YES];
    [[dataSource controller] _didStopLoading:[handle url]];
    
    [currentURL release];
    currentURL = nil;
}

- (void)IFURLHandleResourceDidFinishLoading:(IFURLHandle *)handle data:(NSData *)data
{    
    WEBKIT_ASSERT([currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);
    WEBKIT_ASSERT([handle statusCode] == IFURLHandleStatusLoadComplete);
    WEBKIT_ASSERT((int)[data length] == [handle contentLengthReceived]);

    [dataSource _removeURLHandle:handle];
    
    [loader finish];
    
    IFError *nonTerminalError = [handle error];
    if (nonTerminalError) {
        [[dataSource controller] _receivedError:nonTerminalError forResourceHandle:handle
            partialProgress:[IFLoadProgress progressWithURLHandle:handle] fromDataSource:dataSource];
    }
    
    [self receivedProgressWithHandle:handle complete: YES];

    [[dataSource controller] _didStopLoading:[handle url]];
    
    [currentURL release];
    currentURL = nil;
}

- (void)IFURLHandle:(IFURLHandle *)handle resourceDidFailLoadingWithResult:(IFError *)error
{
#ifdef WEBFOUNDATION_LOAD_MESSAGES_FIXED
    WEBKIT_ASSERT([currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);
#else
    WEBKIT_ASSERT(currentURL == nil || [currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);
#endif    

    [dataSource _removeURLHandle:handle];
    
    [loader cancel];
    
    [[dataSource controller] _receivedError:error forResourceHandle:handle
        partialProgress:[IFLoadProgress progressWithURLHandle:handle] fromDataSource:dataSource];
    [[dataSource controller] _didStopLoading:[handle url]];
    
    [currentURL release];
    currentURL = nil;
}

- (void)IFURLHandle:(IFURLHandle *)handle didRedirectToURL:(NSURL *)URL
{
    WEBKIT_ASSERT(currentURL != nil);
    WEBKIT_ASSERT([URL isEqual:[handle redirectedURL]]);

    [[dataSource controller] _didStopLoading:currentURL];

    [dataSource _setFinalURL:URL];
    [[dataSource _bridge] setURL:URL];

    //[[dataSource _locationChangeHandler] serverRedirectTo:toURL forDataSource:dataSource];
    
    [[dataSource controller] _didStartLoading:URL];
    
    [URL retain];
    [currentURL release];
    currentURL = URL;
}

@end
