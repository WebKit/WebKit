/*	
    IFMainURLHandleClient.mm
	    
    Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFDocument.h>
#import <WebKit/IFDownloadHandler.h>
#import <WebKit/IFHTMLRepresentationPrivate.h>
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
#import <WebKit/WebKitDebug.h>

#import <KWQKHTMLPartImpl.h>

#import <WebFoundation/IFError.h>
#import <WebFoundation/IFFileTypeMappings.h>


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

    [dataSource release];
    [super dealloc];
}

- (void)IFURLHandleResourceDidBeginLoading:(IFURLHandle *)sender
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s\n", DEBUG_OBJECT([sender url]));
    
    WEBKIT_ASSERT(url == nil);
    
    url = [[sender url] retain];
    [(IFWebController *)[dataSource controller] _didStartLoading:url];
}


- (void)IFURLHandleResourceDidCancelLoading:(IFURLHandle *)sender
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s\n", DEBUG_OBJECT([sender url]));
    
    WEBKIT_ASSERT([url isEqual:[sender redirectedURL] ? [sender redirectedURL] : [sender url]]);
    
    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = -1;
    loadProgress->bytesSoFar = -1;
    [(IFWebController *)[dataSource controller] _mainReceivedProgress: (IFLoadProgress *)loadProgress 
        forResourceHandle: sender fromDataSource: dataSource];
    [loadProgress release];
    [(IFWebController *)[dataSource controller] _didStopLoading:url];
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
    
    if(IFContentPolicyShow)
        [[dataSource representation] finishedLoadingWithDataSource:dataSource];
    
    // update progress
    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [data length];
    loadProgress->bytesSoFar = [data length];
    [[dataSource controller] _mainReceivedProgress: (IFLoadProgress *)loadProgress 
        forResourceHandle: sender fromDataSource: dataSource];
    [loadProgress release];
    [[dataSource controller] _didStopLoading:url];
    [url release];
    url = nil;
    
    IFError *nonTerminalError = [sender error];
    if (nonTerminalError){
        [[dataSource controller] _mainReceivedError:nonTerminalError forResourceHandle:sender partialProgress:loadProgress fromDataSource:dataSource];
    }
    
    [downloadHandler finishedLoading];
    [downloadHandler release];
    downloadHandler = nil;
}


- (void)IFURLHandle:(IFURLHandle *)sender resourceDataDidBecomeAvailable:(NSData *)incomingData
{
    int contentLength = [sender contentLength];
    int contentLengthReceived = [sender contentLengthReceived];
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
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "%d of %d", contentLengthReceived, contentLength);
    
    // Don't send the last progress message, it will be sent via
    // IFURLHandleResourceDidFinishLoading
    if (contentLength == contentLengthReceived &&
    	contentLength != -1){
    	return;
    }
    
    // update progress
    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = contentLength;
    loadProgress->bytesSoFar = contentLengthReceived;
    [[dataSource controller] _mainReceivedProgress: (IFLoadProgress *)loadProgress 
        forResourceHandle: sender fromDataSource: dataSource];
    [loadProgress release];
    
    isFirstChunk = NO;
}


- (void)IFURLHandle:(IFURLHandle *)sender resourceDidFailLoadingWithResult:(IFError *)result
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "url = %s, result = %s\n", DEBUG_OBJECT([sender url]), DEBUG_OBJECT([result errorDescription]));

    WEBKIT_ASSERT([url isEqual:[sender redirectedURL] ? [sender redirectedURL] : [sender url]]);

    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [sender contentLength];
    loadProgress->bytesSoFar = [sender contentLengthReceived];

    [[dataSource controller] _mainReceivedError:result forResourceHandle:sender partialProgress:loadProgress fromDataSource:dataSource];
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

    if([dataSource isDocumentHTML]) 
        [(IFHTMLRepresentation *)[dataSource representation] part]->impl->setBaseURL([[url absoluteString] cString]);
    [dataSource _setFinalURL:url];
    
    [[dataSource _locationChangeHandler] serverRedirectTo:url forDataSource:dataSource];
}

@end
