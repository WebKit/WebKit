/*	
    IFMainURLHandleClient.mm
    Copyright (c) 2001, 2002, Apple Computer, Inc. All rights reserved.
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
#import <WebFoundation/IFURLHandle.h>

// FIXME: This is quite similar IFResourceURLHandleClient; they should share code.

@implementation IFMainURLHandleClient

- initWithDataSource: (IFWebDataSource *)ds
{
    self = [super init];
    
    if (self) {
        dataSource = [ds retain];
        isFirstChunk = YES;
    }

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
    WEBKIT_ASSERT(downloadHandler == nil);
    
    [downloadProgressHandler release];
    [dataSource release];
    
    [super dealloc];
}

- (IFDownloadHandler *)downloadHandler
{
    return downloadHandler;
}

- (void)receivedProgressWithHandle:(IFURLHandle *)handle complete:(BOOL)isComplete
{
    IFLoadProgress *progress = [IFLoadProgress progressWithURLHandle:handle];
    
    if ([dataSource contentPolicy] == IFContentPolicySaveAndOpenExternally || [dataSource contentPolicy] == IFContentPolicySave) {
        if (isComplete) {
            [dataSource _setPrimaryLoadComplete:YES];
        }
        [downloadProgressHandler receivedProgress:progress forResourceHandle:handle 
            fromDataSource:dataSource complete:isComplete];
    } else {
        [[dataSource controller] _mainReceivedProgress:progress forResourceHandle:handle 
            fromDataSource:dataSource complete:isComplete];
    }
}

- (void)receivedError:(IFError *)error forHandle:(IFURLHandle *)handle
{
    IFLoadProgress *progress = [IFLoadProgress progressWithURLHandle:handle];

    if ([dataSource contentPolicy] == IFContentPolicySaveAndOpenExternally || [dataSource contentPolicy] == IFContentPolicySave) {
        [downloadProgressHandler receivedError:error forResourceHandle:handle 
            partialProgress:progress fromDataSource:dataSource];
    } else {
        [[dataSource controller] _mainReceivedError:error forResourceHandle:handle 
            partialProgress:progress fromDataSource:dataSource];
    }
}

- (void)IFURLHandleResourceDidBeginLoading:(IFURLHandle *)handle
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "url = %s\n", DEBUG_OBJECT([handle url]));
    
    [self didStartLoadingWithURL:[handle url]];
}

- (void)IFURLHandleResourceDidCancelLoading:(IFURLHandle *)handle
{
    IFError *error;
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "url = %s\n", DEBUG_OBJECT([handle url]));
    
    // FIXME: Maybe we should be passing the URL from the handle here, not from the dataSource.
    error = [[IFError alloc] initWithErrorCode:IFURLHandleResultCancelled 
        inDomain:IFErrorCodeDomainWebFoundation failingURL:[dataSource inputURL]];
    [self receivedError:error forHandle:handle];
    [error release];
    
    [downloadHandler release];
    downloadHandler = nil;

    [self didStopLoading];
}

- (void)IFURLHandleResourceDidFinishLoading:(IFURLHandle *)handle data: (NSData *)data
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "url = %s\n", DEBUG_OBJECT([handle url]));
    
    WEBKIT_ASSERT([currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);
    WEBKIT_ASSERT([handle statusCode] == IFURLHandleStatusLoadComplete);
    WEBKIT_ASSERT((int)[data length] == [handle contentLengthReceived]);
    
    // Don't retain data for downloaded files
    if([dataSource contentPolicy] != IFContentPolicySave &&
       [dataSource contentPolicy] != IFContentPolicySaveAndOpenExternally){
       [dataSource _setResourceData:data];
    }
    
    if([dataSource contentPolicy] == IFContentPolicyShow)
        [[dataSource representation] finishedLoadingWithDataSource:dataSource];
    
    // Either send a final error message or a final progress message.
    IFError *nonTerminalError = [handle error];
    if (nonTerminalError){
        [self receivedError:nonTerminalError forHandle:handle];
    }
    else {
        [self receivedProgressWithHandle:handle complete:YES];
    }
    
    [downloadHandler finishedLoading];
    [downloadHandler release];
    downloadHandler = nil;
    
    [self didStopLoading];
}

- (void)IFURLHandle:(IFURLHandle *)handle resourceDataDidBecomeAvailable:(NSData *)incomingData
{
    IFWebController *controller = [dataSource controller];
    NSString *contentType = [handle contentType];
    IFWebFrame *frame = [dataSource webFrame];
    IFWebView *view = [frame webView];

    NSData *data = nil;
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "url = %s, data = %p, length %d\n", DEBUG_OBJECT([handle url]), incomingData, [incomingData length]);
    
    WEBKIT_ASSERT([currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);
    
    // Check the mime type and ask the client for the content policy.
    if(isFirstChunk){
    
        // Make assumption that if the contentType is the default 
        // and there is no extension, this is text/html
        if([contentType isEqualToString:IFDefaultMIMEType] && [[[currentURL path] pathExtension] isEqualToString:@""])
            contentType = @"text/html";
        
        [dataSource _setContentType:contentType];
        [dataSource _setEncoding:[handle characterSet]];
        
        // retain the downloadProgressHandler just in case this is a download.
        // Alexander releases the IFWebController if no window is created for it.
        // This happens in the cases mentioned in 2981866 and 2965312.
        downloadProgressHandler = [[[dataSource controller] downloadProgressHandler] retain];
        
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
                data = [handle resourceData];
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
            [frame _setProvisionalDataSource:nil];
            [[dataSource _locationChangeHandler] locationChangeDone:nil forDataSource:dataSource];
            downloadHandler = [[IFDownloadHandler alloc] initWithDataSource:dataSource];
        }
        [downloadHandler receivedData:data];
    }
    else if(contentPolicy == IFContentPolicyIgnore){
        [handle cancelLoadInBackground];
        [frame _setProvisionalDataSource:nil];
        [[dataSource _locationChangeHandler] locationChangeDone:nil forDataSource:dataSource];
    }
    else{
        [NSException raise:NSInvalidArgumentException format:
            @"haveContentPolicy: andPath:path forDataSource: set an invalid content policy."];
    }
    
    [self receivedProgressWithHandle:handle complete:NO];
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "%d of %d", [handle contentLengthReceived], [handle contentLength]);
    isFirstChunk = NO;
}

- (void)IFURLHandle:(IFURLHandle *)handle resourceDidFailLoadingWithResult:(IFError *)result
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "url = %s, result = %s\n", DEBUG_OBJECT([handle url]), DEBUG_OBJECT([result errorDescription]));

    WEBKIT_ASSERT([currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);

    [self receivedError:result forHandle:handle];
    
    [downloadHandler cancel];
    [downloadHandler release];
    downloadHandler = nil;

    [self didStopLoading];
}


- (void)IFURLHandle:(IFURLHandle *)handle didRedirectToURL:(NSURL *)URL
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_REDIRECT, "url = %s\n", DEBUG_OBJECT(URL));

    WEBKIT_ASSERT(currentURL != nil);
    WEBKIT_ASSERT([URL isEqual:[handle redirectedURL]]);
    
    [[dataSource _bridge] setURL:URL];
    
    [dataSource _setFinalURL:URL];
    
    [[dataSource _locationChangeHandler] serverRedirectTo:URL forDataSource:dataSource];

    [self didStopLoading];
    [self didStartLoadingWithURL:URL];
}

@end
