/*	
    WebMainResourceClient.mm
    Copyright (c) 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebDocument.h>
#import <WebKit/WebDownloadHandler.h>
#import <WebKit/WebLoadProgress.h>
#import <WebKit/WebLocationChangeHandler.h>
#import <WebKit/WebMainResourceClient.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPolicyHandler.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebView.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebKitDebug.h>

#import <WebFoundation/WebError.h>
#import <WebFoundation/WebFileTypeMappings.h>
#import <WebFoundation/WebResourceHandle.h>

// FIXME: This is quite similar WebSubresourceClient; they should share code.

@implementation WebMainResourceClient

- initWithDataSource: (WebDataSource *)ds
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

- (WebDownloadHandler *)downloadHandler
{
    return downloadHandler;
}

- (void)receivedProgressWithHandle:(WebResourceHandle *)handle complete:(BOOL)isComplete
{
    WebLoadProgress *progress = [WebLoadProgress progressWithResourceHandle:handle];
    
    if ([dataSource contentPolicy] == WebContentPolicySaveAndOpenExternally || [dataSource contentPolicy] == WebContentPolicySave) {
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

- (void)receivedError:(WebError *)error forHandle:(WebResourceHandle *)handle
{
    WebLoadProgress *progress = [WebLoadProgress progressWithResourceHandle:handle];

    if ([dataSource contentPolicy] == WebContentPolicySaveAndOpenExternally || [dataSource contentPolicy] == WebContentPolicySave) {
        [downloadProgressHandler receivedError:error forResourceHandle:handle 
            partialProgress:progress fromDataSource:dataSource];
    } else {
        [[dataSource controller] _mainReceivedError:error forResourceHandle:handle 
            partialProgress:progress fromDataSource:dataSource];
    }
}

- (void)WebResourceHandleDidBeginLoading:(WebResourceHandle *)handle
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "url = %s\n", DEBUG_OBJECT([handle url]));
    
    [self didStartLoadingWithURL:[handle url]];
}

- (void)WebResourceHandleDidCancelLoading:(WebResourceHandle *)handle
{
    WebError *error;
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "url = %s\n", DEBUG_OBJECT([handle url]));
    
    // FIXME: Maybe we should be passing the URL from the handle here, not from the dataSource.
    error = [[WebError alloc] initWithErrorCode:WebResultCancelled 
        inDomain:WebErrorDomainWebFoundation failingURL:[dataSource inputURL]];
    [self receivedError:error forHandle:handle];
    [error release];
    
    [downloadHandler release];
    downloadHandler = nil;

    [self didStopLoading];
}

- (void)WebResourceHandleDidFinishLoading:(WebResourceHandle *)handle data: (NSData *)data
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "url = %s\n", DEBUG_OBJECT([handle url]));
    
    WEBKIT_ASSERT([currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);
    WEBKIT_ASSERT([handle statusCode] == WebResourceHandleStatusLoadComplete);
    WEBKIT_ASSERT((int)[data length] == [handle contentLengthReceived]);
    
    // Don't retain data for downloaded files
    if([dataSource contentPolicy] != WebContentPolicySave &&
       [dataSource contentPolicy] != WebContentPolicySaveAndOpenExternally){
       [dataSource _setResourceData:data];
    }
    
    if([dataSource contentPolicy] == WebContentPolicyShow)
        [[dataSource representation] finishedLoadingWithDataSource:dataSource];
    
    // Either send a final error message or a final progress message.
    WebError *nonTerminalError = [handle error];
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

- (void)WebResourceHandle:(WebResourceHandle *)handle dataDidBecomeAvailable:(NSData *)incomingData
{
    WebController *controller = [dataSource controller];
    NSString *contentType = [handle contentType];
    WebFrame *frame = [dataSource webFrame];

    NSData *data = nil;
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "url = %s, data = %p, length %d\n", DEBUG_OBJECT([handle url]), incomingData, [incomingData length]);
    
    WEBKIT_ASSERT([currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);
    
    // Check the mime type and ask the client for the content policy.
    if(isFirstChunk){
    
        // Make assumption that if the contentType is the default 
        // and there is no extension, this is text/html
        if([contentType isEqualToString:WebDefaultMIMEType] && [[[currentURL path] pathExtension] isEqualToString:@""])
            contentType = @"text/html";
        
        [dataSource _setContentType:contentType];
        [dataSource _setEncoding:[handle characterSet]];
        
        // retain the downloadProgressHandler just in case this is a download.
        // Alexander releases the WebController if no window is created for it.
        // This happens in the cases mentioned in 2981866 and 2965312.
        downloadProgressHandler = [[[dataSource controller] downloadProgressHandler] retain];
        
        [[controller policyHandler] requestContentPolicyForMIMEType:contentType dataSource:dataSource];
        
        WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "main content type: %s", DEBUG_OBJECT(contentType));
    }

    WebContentPolicy contentPolicy = [dataSource contentPolicy];

    if (contentPolicy != WebContentPolicyNone) {
        if (!processedBufferedData && !isFirstChunk) {
            // Process all data that has been received now that we are ready for data
	    data = [handle resourceData];
        } else {
            data = incomingData;
        }
	
	processedBufferedData = YES;          

	switch (contentPolicy) {
	case WebContentPolicyShow:
	    [dataSource _receivedData:data];
	    break;
	case WebContentPolicySave:
	case WebContentPolicySaveAndOpenExternally:
	    if (!downloadHandler) {
		[frame _setProvisionalDataSource:nil];
		[[dataSource _locationChangeHandler] locationChangeDone:nil forDataSource:dataSource];
		downloadHandler = [[WebDownloadHandler alloc] initWithDataSource:dataSource];
	    }
	    [downloadHandler receivedData:data];
	    break;
	case WebContentPolicyIgnore:
	    [handle cancelLoadInBackground];
	    [frame _setProvisionalDataSource:nil];
	    [[dataSource _locationChangeHandler] locationChangeDone:nil forDataSource:dataSource];
	    break;
	default:
	    [NSException raise:NSInvalidArgumentException format:
			     @"haveContentPolicy: andPath:path forDataSource: set an invalid content policy."];
	}
    }

    [self receivedProgressWithHandle:handle complete:NO];
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "%d of %d", [handle contentLengthReceived], [handle contentLength]);
    isFirstChunk = NO;
}

- (void)WebResourceHandle:(WebResourceHandle *)handle didFailLoadingWithResult:(WebError *)result
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "url = %s, result = %s\n", DEBUG_OBJECT([handle url]), DEBUG_OBJECT([result errorDescription]));

    WEBKIT_ASSERT([currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);

    [self receivedError:result forHandle:handle];
    
    [downloadHandler cancel];
    [downloadHandler release];
    downloadHandler = nil;

    [self didStopLoading];
}


- (void)WebResourceHandle:(WebResourceHandle *)handle didRedirectToURL:(NSURL *)URL
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_REDIRECT, "url = %s\n", DEBUG_OBJECT(URL));

    WEBKIT_ASSERT(currentURL != nil);
    WEBKIT_ASSERT([URL isEqual:[handle redirectedURL]]);
    
    [dataSource _setFinalURL:URL];

    [self didStopLoading];
    [self didStartLoadingWithURL:URL];
}

@end
