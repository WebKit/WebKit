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
#import <WebKit/IFWebControllerPolicyHandler.h>
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

// FIXME: This is quite similar IFResourceURLHandleClient; they should share code.

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
    WEBKIT_ASSERT(url == nil);
    [downloadProgressHandler release];
    [dataSource release];
    [super dealloc];
}

- (IFDownloadHandler *) downloadHandler
{
    return downloadHandler;
}

- (void)_receivedProgress: (IFLoadProgress *)progress forResourceHandle: (IFURLHandle *)resourceHandle fromDataSource: (IFWebDataSource *)theDataSource complete: (BOOL)isComplete
{
    if([dataSource contentPolicy] == IFContentPolicySaveAndOpenExternally || [dataSource contentPolicy] == IFContentPolicySave){
        if(isComplete)
            [dataSource _setPrimaryLoadComplete: YES];
            
        if (progress->bytesSoFar == -1 && progress->totalToLoad == -1){  
            IFError *error = [[IFError alloc] initWithErrorCode: IFURLHandleResultCancelled 
                inDomain:IFErrorCodeDomainWebFoundation failingURL: [dataSource inputURL]];
            [dataSource _setMainDocumentError: error];
            [downloadProgressHandler receivedError: error forResourceHandle: resourceHandle partialProgress: progress fromDataSource: dataSource];
            [error release];
        }
        
        [downloadProgressHandler receivedProgress:progress forResourceHandle:resourceHandle 
            fromDataSource:theDataSource complete:isComplete];
    }else{
        [[dataSource controller] _mainReceivedProgress:progress forResourceHandle:resourceHandle 
            fromDataSource:theDataSource complete:isComplete];
    }
}

- (void)_receivedError: (IFError *)error forResourceHandle: (IFURLHandle *)resourceHandle partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)theDataSource
{
    if([dataSource contentPolicy] == IFContentPolicySaveAndOpenExternally || [dataSource contentPolicy] == IFContentPolicySave){
        [downloadProgressHandler receivedError:error forResourceHandle:resourceHandle 
            partialProgress:progress fromDataSource:theDataSource];
    }else{
        [[dataSource controller] _mainReceivedError:error forResourceHandle:resourceHandle 
            partialProgress:progress fromDataSource:theDataSource];
    }
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

    WEBKIT_ASSERT([url isEqual:[sender redirectedURL] ? [sender redirectedURL] : [sender url]]);
    
    [self _receivedProgress:[IFLoadProgress progress] forResourceHandle:sender fromDataSource: dataSource complete: YES];
    
    [[dataSource controller] _didStopLoading:url];
    
    [url release];
    url = nil;
    
    [downloadHandler release];
    downloadHandler = nil;
}


- (void)IFURLHandleResourceDidFinishLoading:(IFURLHandle *)sender data: (NSData *)data
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s\n", DEBUG_OBJECT([sender url]));
    
    WEBKIT_ASSERT([url isEqual:[sender redirectedURL] ? [sender redirectedURL] : [sender url]]);
    
    // Don't retain data for downloaded files
    if([dataSource contentPolicy] != IFContentPolicySave &&
       [dataSource contentPolicy] != IFContentPolicySaveAndOpenExternally){
       [dataSource _setResourceData:data];
    }
    
    if([dataSource contentPolicy] == IFContentPolicyShow)
        [[dataSource representation] finishedLoadingWithDataSource:dataSource];
    
    // Either send a final error message or a final progress message.
    IFError *nonTerminalError = [sender error];
    if (nonTerminalError){
        [self _receivedError:nonTerminalError forResourceHandle:sender 
            partialProgress:[IFLoadProgress progressWithURLHandle:sender] fromDataSource:dataSource];
    }
    else {
        // update progress
        [self _receivedProgress:[IFLoadProgress progressWithURLHandle:sender]
            forResourceHandle:sender fromDataSource: dataSource complete: YES];
    }
    
    [[dataSource controller] _didStopLoading:url];

    [url release];
    url = nil;    
    
    [downloadHandler finishedLoading];
    [downloadHandler release];
    downloadHandler = nil;
}


- (void)IFURLHandle:(IFURLHandle *)sender resourceDataDidBecomeAvailable:(NSData *)incomingData
{
    IFWebController *controller = [dataSource controller];
    NSString *contentType = [sender contentType];
    IFWebFrame *frame = [dataSource webFrame];
    IFWebView *view = [frame webView];

    NSData *data = nil;
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "url = %s, data = %p, length %d\n", DEBUG_OBJECT([sender url]), incomingData, [incomingData length]);
    
    WEBKIT_ASSERT([url isEqual:[sender redirectedURL] ? [sender redirectedURL] : [sender url]]);
    
    // Check the mime type and ask the client for the content policy.
    if(isFirstChunk){
    
        // Make assumption that if the contentType is the default 
        // and there is no extension, this is text/html
        if([contentType isEqualToString:IFDefaultMIMEType] && [[[url path] pathExtension] isEqualToString:@""])
            contentType = @"text/html";
        
        [dataSource _setContentType:contentType];
        [dataSource _setEncoding:[sender characterSet]];
        [[controller policyHandler] requestContentPolicyForMIMEType:contentType dataSource:dataSource];
        
        WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "main content type: %s", DEBUG_OBJECT(contentType));
    }
    
    IFContentPolicy contentPolicy = [dataSource contentPolicy];

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
    }
    else if(contentPolicy == IFContentPolicySave || contentPolicy == IFContentPolicySaveAndOpenExternally){
        if(!downloadHandler){
            downloadProgressHandler = [[[dataSource controller] downloadProgressHandler] retain];
            [frame->_private setProvisionalDataSource:nil];
            [[dataSource _locationChangeHandler] locationChangeDone:nil forDataSource:dataSource];
            downloadHandler = [[IFDownloadHandler alloc] initWithDataSource:dataSource];
        }
        [downloadHandler receivedData:data];
    }
    else if(contentPolicy == IFContentPolicyIgnore){
        [sender cancelLoadInBackground];
        [frame->_private setProvisionalDataSource:nil];
        [[dataSource _locationChangeHandler] locationChangeDone:nil forDataSource:dataSource];
    }
    else{
        [NSException raise:NSInvalidArgumentException format:
            @"haveContentPolicy: andPath:path forDataSource: set an invalid content policy."];
    }
    
    //update progress
    [self _receivedProgress:[IFLoadProgress progressWithURLHandle:sender] 
        forResourceHandle:sender fromDataSource: dataSource complete: NO];
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "%d of %d", [sender contentLengthReceived], [sender contentLength]);
    isFirstChunk = NO;
}


- (void)IFURLHandle:(IFURLHandle *)sender resourceDidFailLoadingWithResult:(IFError *)result
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s, result = %s\n", DEBUG_OBJECT([sender url]), DEBUG_OBJECT([result errorDescription]));

    WEBKIT_ASSERT([url isEqual:[sender redirectedURL] ? [sender redirectedURL] : [sender url]]);

    [self _receivedError:result forResourceHandle:sender 
        partialProgress:[IFLoadProgress progressWithURLHandle:sender] fromDataSource:dataSource];
    
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
