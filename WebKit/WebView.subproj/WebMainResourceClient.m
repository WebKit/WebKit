/*	
    IFMainURLHandleClient.mm
    Copyright (c) 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFDocument.h>
#import <WebKit/IFDownloadHandler.h>
#import <WebKit/IFLoadProgress.h>
#import <WebKit/IFLocationChangeHandler.h>
#import <WebKit/IFMainURLHandleClient.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFWebControllerPrivate.h>
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebFrame.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFWebView.h>
#import <WebKit/IFWebCoreBridge.h>
#import <WebKit/WebKitDebug.h>

#import <WebFoundation/IFError.h>
#import <WebFoundation/IFFileTypeMappings.h>

// FIXME: This is almost completely redundant with the KWQURLLoadClient in WebCore.
// We shouldn't have two almost-identical classes that don't share code.

@implementation IFMainURLHandleClient

- initWithDataSource: (IFWebDataSource *)ds
{
    if ((self = [super init])) {
        dataSource = [ds retain];
        processedBufferedData = NO;
        isFirstChunk = YES;
        return self;
    }

    return nil;
}

- (void)dealloc
{
    // FIXME Radar 2954901: Changed this not to leak because cancel messages are sometimes sent before begin.
#ifdef WEBFOUNDATION_LOAD_MESSAGES_FIXED    
    WEBKIT_ASSERT(url == nil);
#else
    [url release];
#endif    
    [dataSource release];
    [super dealloc];
}

- (void)IFURLHandleResourceDidBeginLoading:(IFURLHandle *)sender
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s\n", DEBUG_OBJECT([sender url]));
    
    WEBKIT_ASSERT(url == nil);
    
    url = [[sender url] retain];
    [[dataSource controller] _didStartLoading:url];
}


- (void)IFURLHandleResourceDidCancelLoading:(IFURLHandle *)sender
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s\n", DEBUG_OBJECT([sender url]));

    // FIXME Radar 2954901: I replaced the assertion below with a more lenient one,
    // since cancel messages are sometimes sent before begin.
#ifdef WEBFOUNDATION_LOAD_MESSAGES_FIXED    
    WEBKIT_ASSERT([url isEqual:[sender redirectedURL] ? [sender redirectedURL] : [sender url]]);
#else
    WEBKIT_ASSERT(url == nil || [url isEqual:[sender redirectedURL] ? [sender redirectedURL] : [sender url]]);
#endif    
    
    [[dataSource controller] _mainReceivedProgress:[IFLoadProgress progress]
        forResourceHandle:sender fromDataSource: dataSource complete: YES];
    [[dataSource controller] _didStopLoading:url];
    [url release];
    url = nil;
    
    [downloadHandler cancel];
    [downloadHandler release];
    downloadHandler = nil;
}


- (void)IFURLHandleResourceDidFinishLoading:(IFURLHandle *)sender data: (NSData *)data
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s\n", DEBUG_OBJECT([sender url]));
    
    WEBKIT_ASSERT([url isEqual:[sender redirectedURL] ? [sender redirectedURL] : [sender url]]);
    
    // Don't retain download data
    if([dataSource contentPolicy] != IFContentPolicySave &&
       [dataSource contentPolicy] != IFContentPolicyOpenExternally){
       [dataSource _setResourceData:data];
    }
    
    if([dataSource contentPolicy] == IFContentPolicyShow)
        [[dataSource representation] finishedLoadingWithDataSource:dataSource];
    
    // update progress
    [[dataSource controller] _mainReceivedProgress:[IFLoadProgress progressWithURLHandle:sender]
        forResourceHandle:sender fromDataSource:dataSource complete:YES];
    [[dataSource controller] _didStopLoading:url];
    [url release];
    url = nil;
    
    IFError *nonTerminalError = [sender error];
    if (nonTerminalError){
        [[dataSource controller] _mainReceivedError:nonTerminalError forResourceHandle:sender partialProgress:[IFLoadProgress progressWithURLHandle:sender] fromDataSource:dataSource];
    }
    
    [downloadHandler finishedLoading];
    [downloadHandler release];
    downloadHandler = nil;
}


- (void)IFURLHandle:(IFURLHandle *)sender resourceDataDidBecomeAvailable:(NSData *)incomingData
{
    NSString *contentType = [sender contentType];
    IFWebFrame *frame = [dataSource webFrame];
    IFWebView *view = [frame webView];
    IFContentPolicy contentPolicy;
    NSData *data = nil;
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "url = %s, data = %p, length %d\n", DEBUG_OBJECT([sender url]), incomingData, [incomingData length]);
    
    WEBKIT_ASSERT([url isEqual:[sender redirectedURL] ? [sender redirectedURL] : [sender url]]);
    
    // Check the mime type and ask the client for the content policy.
    if(isFirstChunk){
        // Make assumption here that if the contentType is the default 
        // and there is no extension, this is text/html
        if([contentType isEqualToString:IFDefaultMIMEType] && [[[url path] pathExtension] isEqualToString:@""])
            contentType = @"text/html";
        WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "main content type: %s", DEBUG_OBJECT(contentType));
        
        [dataSource _setContentType:contentType];
        [dataSource _setEncoding:[sender characterSet]];
        [[dataSource _locationChangeHandler] requestContentPolicyForMIMEType:contentType];
    }
    
    contentPolicy = [dataSource contentPolicy];

    if(contentPolicy != IFContentPolicyNone){
        if(!processedBufferedData){
            // Process all data that has been received now that we have a content policy
            // and don't call resourceData if this is the first chunk since resourceData is a copy
            if(isFirstChunk){
                data = incomingData;
            }else{
                data = [sender resourceData];
            }
            processedBufferedData = YES;          
        }else{
            data = incomingData;
        }
    }
    
    if(contentPolicy == IFContentPolicyShow){
        [[dataSource representation] receivedData:data withDataSource:dataSource];
        [[view documentView] dataSourceUpdated:dataSource];
        
    }else if(contentPolicy == IFContentPolicySave || contentPolicy == IFContentPolicyOpenExternally){
        if(!downloadHandler){
            [frame->_private setProvisionalDataSource:nil];
            [[dataSource _locationChangeHandler] locationChangeDone:nil];
            downloadHandler = [[IFDownloadHandler alloc] initWithDataSource:dataSource];
        }
        [downloadHandler receivedData:data];
        
    }else if(contentPolicy == IFContentPolicyIgnore){
        [sender cancelLoadInBackground];
        return;
    }
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "%d of %d", [sender contentLengthReceived], [sender contentLength]);
    
    // update progress
    [[dataSource controller] _mainReceivedProgress:[IFLoadProgress progressWithURLHandle:sender]
        forResourceHandle:sender fromDataSource:dataSource complete: NO];
    
    isFirstChunk = NO;
}


- (void)IFURLHandle:(IFURLHandle *)sender resourceDidFailLoadingWithResult:(IFError *)result
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s, result = %s\n", DEBUG_OBJECT([sender url]), DEBUG_OBJECT([result errorDescription]));

    WEBKIT_ASSERT([url isEqual:[sender redirectedURL] ? [sender redirectedURL] : [sender url]]);

    [[dataSource controller] _mainReceivedError:result forResourceHandle:sender partialProgress:[IFLoadProgress progressWithURLHandle:sender] fromDataSource:dataSource];
    [[dataSource controller] _didStopLoading:url];
    [url release];
    url = nil;
    
    [downloadHandler cancel];
    [downloadHandler release];
    downloadHandler = nil;
}


- (void)IFURLHandle:(IFURLHandle *)sender didRedirectToURL:(NSURL *)newURL
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_REDIRECT, "url = %s\n", DEBUG_OBJECT(newURL));
    
    WEBKIT_ASSERT(url != nil);
    
    [[dataSource controller] _didStopLoading:url];
    [newURL retain];
    [url release];
    url = newURL;
    [[dataSource controller] _didStartLoading:url];

    [[dataSource _bridge] setURL:url];
    [dataSource _setFinalURL:url];
    
    [[dataSource _locationChangeHandler] serverRedirectTo:url forDataSource:dataSource];
}

@end
