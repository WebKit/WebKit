/*	
    WebMainResourceClient.mm
    Copyright (c) 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebBridge.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDownloadHandler.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebLocationChangeDelegate.h>
#import <WebKit/WebMainResourceClient.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebView.h>

#import <WebFoundation/WebError.h>
#import <WebFoundation/WebFileTypeMappings.h>
#import <WebFoundation/WebResourceHandle.h>
#import <WebFoundation/WebResourceRequest.h>
#import <WebFoundation/WebHTTPResourceRequest.h>
#import <WebFoundation/WebResourceHandlePrivate.h>
#import <WebFoundation/WebResourceResponse.h>
#import <WebFoundation/WebCookieConstants.h>

// FIXME: This is quite similar to WebSubresourceClient; they should share code.

@implementation WebMainResourceClient

- initWithDataSource:(WebDataSource *)ds
{
    self = [super init];
    
    if (self) {
        dataSource = [ds retain];
        resourceData = [[NSMutableData alloc] init];
        
        // set the user agent for the request
        // consult the data source's controller
        WebController *controller = [dataSource controller];
        resourceProgressDelegate = [[controller resourceProgressDelegate] retain];
    }

    return self;
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

- (void)dealloc
{
    ASSERT(currentURL == nil);
    ASSERT(downloadHandler == nil);
    
    [downloadProgressDelegate release];
    [resourceProgressDelegate release];
    [resourceData release];
    [dataSource release];
    [response release];
    [request release];
    
    [super dealloc];
}

- (NSData *)resourceData
{
    return resourceData;
}

- (WebDownloadHandler *)downloadHandler
{
    return downloadHandler;
}

- (void)receivedProgressWithHandle:(WebResourceHandle *)handle complete:(BOOL)isComplete
{
    WebContentAction contentAction = [[dataSource contentPolicy] policyAction];

    if (contentAction == WebContentPolicySaveAndOpenExternally || contentAction == WebContentPolicySave) {
        if (isComplete) {
            [dataSource _setPrimaryLoadComplete:YES];
        }
    } else {
        [[dataSource controller] _mainReceivedProgressForResourceHandle:handle 
            bytesSoFar: [resourceData length] fromDataSource:dataSource complete:isComplete];
    }
}

- (void)receivedError:(WebError *)error forHandle:(WebResourceHandle *)handle
{
    if(suppressErrors){
        return;
    }
    
    WebContentAction contentAction = [[dataSource contentPolicy] policyAction];

    if (contentAction == WebContentPolicySaveAndOpenExternally || contentAction == WebContentPolicySave) {
        [downloadProgressDelegate resourceRequest: [handle _request] didFailLoadingWithError:error fromDataSource:dataSource];
    } else {
        [[dataSource controller] _mainReceivedError:error forResourceHandle:handle 
            fromDataSource:dataSource];
    }
}

- (void)didCancelWithHandle:(WebResourceHandle *)handle
{
    if (currentURL == nil) {
        return;
    }
    
    LOG(Loading, "URL = %@", currentURL);
    
    // Calling receivedError will likely result in a call to release, so we must retain.
    [self retain];
    
    // FIXME: Maybe we should be passing the URL from the handle here, not from the dataSource.
    WebError *error = [[WebError alloc] initWithErrorCode:WebErrorCodeCancelled
                                                 inDomain:WebErrorDomainWebFoundation
                                               failingURL:[[[dataSource request] URL] absoluteString]];
    [self receivedError:error forHandle:handle];
    [error release];

    if (downloadHandler) {
        WebError *downloadError = [downloadHandler cancel];
        if(downloadError) {
            [self receivedError:downloadError forHandle:handle];
        }
        [downloadHandler release];
        downloadHandler = nil;
    }
    
    [self didStopLoading];
    
    [self release];
}

- (void)handleDidFinishLoading:(WebResourceHandle *)handle
{
    LOG(Loading, "URL = %@", currentURL);
    
    // Calling receivedError will likely result in a call to release, so we must retain.
    [self retain];

    WebContentAction contentAction = [[dataSource contentPolicy] policyAction];
    
    // Don't retain data for downloaded files
    if(contentAction != WebContentPolicySave && contentAction != WebContentPolicySaveAndOpenExternally){
       [dataSource _setResourceData:resourceData];
    }

    if (contentAction == WebContentPolicyShow) {
        [[dataSource representation] finishedLoadingWithDataSource:dataSource];
    }
    
    // Either send a final error message or a final progress message.
    WebError *nonTerminalError = [[dataSource response] error];
    if (nonTerminalError) {
        [self receivedError:nonTerminalError forHandle:handle];
    } else {
        [self receivedProgressWithHandle:handle complete:YES];
    }

    if (downloadHandler) {
        WebError *downloadError = [downloadHandler finishedLoading];
        if (downloadError) {
            [self receivedError:downloadError forHandle:handle];
        }
        [downloadHandler release];
        downloadHandler = nil;
        [downloadProgressDelegate resourceRequest:[handle _request] didFinishLoadingFromDataSource:dataSource];
    }
    else
        [resourceProgressDelegate resourceRequest:[handle _request] didFinishLoadingFromDataSource:dataSource];
    
    [self didStopLoading];

    
    [self release];
}

-(WebResourceRequest *)handle:(WebResourceHandle *)handle willSendRequest:(WebResourceRequest *)newRequest
{
    WebController *controller = [dataSource controller];
    NSURL *URL = [newRequest URL];

    LOG(Redirect, "URL = %@", URL);

    // FIXME: need to update main document URL here, or cookies set
    // via redirects might not work in main document mode

    [newRequest setUserAgent:[controller userAgentForURL:URL]];

    // Don't set this on the first request.  It is set
    // when the main load was started.
    if (request)
        [dataSource _setRequest:newRequest];

    // Not the first send, so reload.
    if (request) {
        [self didStopLoading];
        [self didStartLoadingWithURL:URL];
    }

    // Let the resourceProgressDelegate get a crack at modifying the request.
    newRequest = [resourceProgressDelegate resourceRequest: request willSendRequest: newRequest fromDataSource: dataSource];

    WebResourceRequest *oldRequest = request;
    request = [newRequest copy];
    [oldRequest release];
        
    return newRequest;
}

-(void)handle:(WebResourceHandle *)handle didReceiveResponse:(WebResourceResponse *)r
{
    NSString *contentType = [r contentType];

    ASSERT (response == nil);
    
    response = [r retain];
    
    [dataSource _setResponse:response];

    // Make assumption that if the contentType is the default and there is no extension, this is text/html.
    if ([contentType isEqualToString:@"application/octet-stream"]
            && [[[currentURL path] pathExtension] isEqualToString:@""])
        contentType = @"text/html";
    LOG(Download, "main content type: %@", contentType);

    // Retain the downloadProgressDelegate just in case this is a download.
    // Alexander releases the WebController if no window is created for it.
    // This happens in the cases mentioned in 2981866 and 2965312.
    downloadProgressDelegate = [[[dataSource controller] downloadProgressDelegate] retain];

    // Figure out the content policy.
    WebContentPolicy *contentPolicy = [dataSource contentPolicy];
    if (contentPolicy == nil) {
        contentPolicy = [[[dataSource controller] policyDelegate]
            contentPolicyForMIMEType:contentType URL:currentURL inFrame:[dataSource webFrame]];
        [dataSource _setContentPolicy:contentPolicy];
    }
    policyAction = [contentPolicy policyAction];

    switch (policyAction) {
    case WebContentPolicyShow:
        [resourceProgressDelegate resourceRequest: request didReceiveResponse: response fromDataSource: dataSource];
        break;
    case WebContentPolicySave:
    case WebContentPolicySaveAndOpenExternally:
        [[dataSource webFrame] _setProvisionalDataSource:nil];
        [[[dataSource controller] locationChangeDelegate] locationChangeDone:nil forDataSource:dataSource];
        downloadHandler = [[WebDownloadHandler alloc] initWithDataSource:dataSource];
        [downloadProgressDelegate resourceRequest: request didReceiveResponse: response fromDataSource: dataSource];
        break;
    case WebContentPolicyIgnore:
        [handle cancel];
        [self didCancelWithHandle:handle];
        [[dataSource webFrame] _setProvisionalDataSource:nil];
	[[[dataSource controller] locationChangeDelegate] locationChangeDone:nil forDataSource:dataSource];
        break;
    default:
        ERROR("contentPolicyForMIMEType:URL:inFrame: returned an invalid content policy.");
    }
}

- (void)handle:(WebResourceHandle *)handle didReceiveData:(NSData *)data
{
    LOG(Loading, "URL = %@, data = %p, length %d", currentURL, data, [data length]);
    
    WebError *downloadError = nil;
    

    if (downloadHandler) {
        downloadError = [downloadHandler receivedData:data];
        [downloadProgressDelegate resourceRequest: request didReceiveContentLength: [data length] fromDataSource:dataSource];
    } else {
        [resourceData appendData:data];
        [dataSource _receivedData:data];
        [resourceProgressDelegate resourceRequest: request didReceiveContentLength: [data length] fromDataSource:dataSource];
    }

    [self receivedProgressWithHandle:handle complete:NO];

    if (downloadError) {
        [self receivedError:downloadError forHandle:handle];

        // Supress errors because we don't want to confuse the client with
        // the cancel error that will follow after cancel.
        suppressErrors = YES;
        [handle cancel];
    }
    
    LOG(Download, "%d of %d", [response contentLengthReceived], [response contentLength]);
}

- (void)handle:(WebResourceHandle *)handle didFailLoadingWithError:(WebError *)result
{
    LOG(Loading, "URL = %@, result = %@", [result failingURL], [result errorDescription]);

    if (!downloadHandler)
        [resourceProgressDelegate resourceRequest: request didFailLoadingWithError: result fromDataSource: dataSource];

    // Calling receivedError will likely result in a call to release, so we must retain.
    [self retain];

    [self receivedError:result forHandle:handle];

    if (downloadHandler) {
        WebError *downloadError = [downloadHandler cancel];
        if (downloadError) {
            [self receivedError:downloadError forHandle:handle];
        }
        [downloadHandler release];
        downloadHandler = nil;
    }

    [self didStopLoading];
    
    [self release];
}

@end
