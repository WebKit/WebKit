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
    WebContentAction contentAction = [[dataSource contentPolicy] policyAction];

    
    if (contentAction == WebContentPolicySaveAndOpenExternally || contentAction == WebContentPolicySave) {
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
    WebContentAction contentAction = [[dataSource contentPolicy] policyAction];

    if (contentAction == WebContentPolicySaveAndOpenExternally || contentAction == WebContentPolicySave) {
        [downloadProgressHandler receivedError:error forResourceHandle:handle 
            partialProgress:progress fromDataSource:dataSource];
    } else {
        [[dataSource controller] _mainReceivedError:error forResourceHandle:handle 
            partialProgress:progress fromDataSource:dataSource];
    }
}

- (void)WebResourceHandleDidBeginLoading:(WebResourceHandle *)handle
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "URL = %s\n", DEBUG_OBJECT([handle URL]));
    
    [self didStartLoadingWithURL:[handle URL]];
}

- (void)WebResourceHandleDidCancelLoading:(WebResourceHandle *)handle
{
    WebError *error;
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "URL = %s\n", DEBUG_OBJECT([handle URL]));
    
    // FIXME: Maybe we should be passing the URL from the handle here, not from the dataSource.
    error = [[WebError alloc] initWithErrorCode:WebResultCancelled 
        inDomain:WebErrorDomainWebFoundation failingURL:[[dataSource originalURL] absoluteString]];
    [self receivedError:error forHandle:handle];
    [error release];
    
    [downloadHandler release];
    downloadHandler = nil;

    [self didStopLoading];
}

- (void)WebResourceHandleDidFinishLoading:(WebResourceHandle *)handle data: (NSData *)data
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "URL = %s\n", DEBUG_OBJECT([handle URL]));
    
    WEBKIT_ASSERT([currentURL isEqual:[handle URL]]);
    WEBKIT_ASSERT([handle statusCode] == WebResourceHandleStatusLoadComplete);
    WEBKIT_ASSERT((int)[data length] == [handle contentLengthReceived]);

    WebContentAction contentAction = [[dataSource contentPolicy] policyAction];
    
    // Don't retain data for downloaded files
    if(contentAction != WebContentPolicySave && contentAction != WebContentPolicySaveAndOpenExternally){
       [dataSource _setResourceData:data];
    }

    if(contentAction == WebContentPolicyShow){
        [[dataSource representation] finishedLoadingWithDataSource:dataSource];
    }
    
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

- (void)WebResourceHandle:(WebResourceHandle *)handle dataDidBecomeAvailable:(NSData *)data
{
    WebController *controller = [dataSource controller];
    NSString *contentType = [handle contentType];
    WebFrame *frame = [dataSource webFrame];
    WebContentPolicy *contentPolicy;
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "URL = %s, data = %p, length %d\n", DEBUG_OBJECT([handle URL]), data, [data length]);
    
    WEBKIT_ASSERT([currentURL isEqual:[handle URL]]);
    
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

        if(![dataSource contentPolicy]){
            contentPolicy = [[controller policyHandler] contentPolicyForMIMEType: contentType dataSource: dataSource];
            [dataSource _setContentPolicy:contentPolicy];
        }
        policyAction = [[dataSource contentPolicy] policyAction];
        
        WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "main content type: %s", DEBUG_OBJECT(contentType));
    }

    switch (policyAction) {
    case WebContentPolicyShow:
        [dataSource _receivedData:data];
        break;
    case WebContentPolicySave:
    case WebContentPolicySaveAndOpenExternally:
        if (!downloadHandler) {
            [frame _setProvisionalDataSource:nil];
	    [[[dataSource controller] locationChangeHandler] locationChangeDone:nil forDataSource:dataSource];
            downloadHandler = [[WebDownloadHandler alloc] initWithDataSource:dataSource];
        }
        [downloadHandler receivedData:data];
        break;
    case WebContentPolicyIgnore:
        [handle cancelLoadInBackground];
        [frame _setProvisionalDataSource:nil];
	[[[dataSource controller] locationChangeHandler] locationChangeDone:nil forDataSource:dataSource];
        break;
    default:
        [NSException raise:NSInvalidArgumentException format:
             @"contentPolicyForMIMEType: dataSource: returned an invalid content policy."];
    }

    [self receivedProgressWithHandle:handle complete:NO];
    
    WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "%d of %d", [handle contentLengthReceived], [handle contentLength]);
    isFirstChunk = NO;
}

- (void)WebResourceHandle:(WebResourceHandle *)handle didFailLoadingWithResult:(WebError *)result
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_LOADING, "URL = %s, result = %s\n", DEBUG_OBJECT([handle URL]), DEBUG_OBJECT([result errorDescription]));

    WEBKIT_ASSERT([currentURL isEqual:[handle URL]]);

    [self receivedError:result forHandle:handle];
    
    [downloadHandler cancel];
    [downloadHandler release];
    downloadHandler = nil;

    [self didStopLoading];
}


- (void)WebResourceHandle:(WebResourceHandle *)handle didRedirectToURL:(NSURL *)URL
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_REDIRECT, "URL = %s\n", DEBUG_OBJECT(URL));

    WEBKIT_ASSERT(currentURL != nil);
    WEBKIT_ASSERT([URL isEqual:[handle URL]]);
    
    [dataSource _setURL:URL];

    [self didStopLoading];
    [self didStartLoadingWithURL:URL];
}

@end
