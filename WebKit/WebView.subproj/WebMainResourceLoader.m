/*	
    WebMainResourceClient.m
    Copyright (c) 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebBridge.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDownloadHandler.h>
#import <WebKit/WebKitErrors.h>
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
        resourceProgressDelegate = [[controller resourceLoadDelegate] retain];
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

- (void)receivedError:(WebError *)error forHandle:(WebResourceHandle *)handle
{    
    WebContentAction contentAction = [[dataSource contentPolicy] policyAction];

    if (contentAction == WebContentPolicySaveAndOpenExternally || contentAction == WebContentPolicySave) {
        [downloadProgressDelegate resource: identifier didFailLoadingWithError:error fromDataSource:dataSource];
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
        [downloadHandler cancel];
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
    if (contentAction != WebContentPolicySave && contentAction != WebContentPolicySaveAndOpenExternally) {
    	[dataSource _setResourceData:resourceData];
    }

    if (contentAction == WebContentPolicyShow) {
        [[dataSource representation] finishedLoadingWithDataSource:dataSource];
    }
    
    if (downloadHandler) {
        WebError *downloadError = [downloadHandler finishedLoading];
        if (downloadError) {
            [self receivedError:downloadError forHandle:handle];
        }else{
            [downloadProgressDelegate resource:identifier didFinishLoadingFromDataSource:dataSource];
        }
        [dataSource _setPrimaryLoadComplete:YES];
        [downloadHandler release];
        downloadHandler = nil;
    }
    else {
        [dataSource _finishedLoading];
        [resourceProgressDelegate resource:identifier didFinishLoadingFromDataSource:dataSource];

        // FIXME: Please let Chris know if this is really necessary?
        // Either send a final error message or a final progress message.
        WebError *nonTerminalError = [[dataSource response] error];
        if (nonTerminalError) {
            [self receivedError:nonTerminalError forHandle:handle];
        } else {
            [[dataSource controller] _mainReceivedProgressForResourceHandle:handle
                                                                 bytesSoFar:[resourceData length]
                                                             fromDataSource:dataSource
                                                                   complete:YES];
        }
    }

    [identifier release];
    identifier = nil;
        
    [self didStopLoading];
    
    [self release];
}

-(WebResourceRequest *)handle:(WebResourceHandle *)handle willSendRequest:(WebResourceRequest *)newRequest
{
    WebResourceRequest *result;

    [newRequest setUserAgent:[[dataSource controller] userAgentForURL:[newRequest URL]]];

    // Let the resourceProgressDelegate get a crack at modifying the request.
    if (resourceProgressDelegate) {
        if (identifier == nil){
            // The identifier is released after the last callback, rather than in dealloc
            // to avoid potential cycles.
            identifier = [[resourceProgressDelegate identifierForInitialRequest: newRequest fromDataSource: dataSource] retain];
        }
        newRequest = [resourceProgressDelegate resource: identifier willSendRequest: newRequest fromDataSource: dataSource];
    }
    
    ASSERT(newRequest != nil);

    NSURL *URL = [newRequest URL];

    if (![[dataSource webFrame] _shouldShowURL:URL]) {
        [handle cancel];
        [[dataSource webFrame] _setProvisionalDataSource:nil];
	[[[dataSource controller] locationChangeDelegate] locationChangeDone:[WebError errorWithCode:WebErrorLocationChangeInterruptedByURLPolicyChange inDomain:WebErrorDomainWebKit failingURL:nil] forDataSource:dataSource];
        result = nil;
    }
    else {
        LOG(Redirect, "URL = %@", URL);
    
        // Update cookie policy base URL as URL changes, except for subframes, which use the
        // URL of the main frame which doesn't change when we redirect.
        if ([dataSource webFrame] == [[dataSource controller] mainFrame]) {
            [newRequest setCookiePolicyBaseURL:URL];
        }
    
        WebResourceRequest *copy = [newRequest copy];
    
        // Don't set this on the first request.  It is set
        // when the main load was started.
        if (request)
            [dataSource _setRequest:copy];
    
        // Not the first send, so reload.
        if (request) {
            [self didStopLoading];
            [self didStartLoadingWithURL:URL];
        }
    
        [request release];
        result = request = copy;
    }
        
    return result;
}

-(void)handle:(WebResourceHandle *)handle didReceiveResponse:(WebResourceResponse *)r
{
    ASSERT (response == nil);
    
    response = [r retain];
    
    [dataSource _setResponse:response];

    LOG(Download, "main content type: %@", [response contentType]);

    // Retain the downloadProgressDelegate just in case this is a download.
    // Alexander releases the WebController if no window is created for it.
    // This happens in the cases mentioned in 2981866 and 2965312.
    downloadProgressDelegate = [[[dataSource controller] downloadDelegate] retain];

    // Figure out the content policy.
    WebContentPolicy *contentPolicy = [dataSource contentPolicy];
    contentPolicy = [[[dataSource controller] policyDelegate] contentPolicyForResponse:response
                                                                                andURL:currentURL
                                                                               inFrame:[dataSource webFrame]
                                                                     withContentPolicy:contentPolicy];
    [dataSource _setContentPolicy:contentPolicy];

    policyAction = [contentPolicy policyAction];

    switch (policyAction) {
    case WebContentPolicyShow:
        [resourceProgressDelegate resource: identifier didReceiveResponse: response fromDataSource: dataSource];
        break;
    case WebContentPolicySave:
    case WebContentPolicySaveAndOpenExternally:
        [[dataSource webFrame] _setProvisionalDataSource:nil];
        [[[dataSource controller] locationChangeDelegate] locationChangeDone:nil forDataSource:dataSource];
        downloadHandler = [[WebDownloadHandler alloc] initWithDataSource:dataSource];
        WebError *downloadError = [downloadHandler receivedResponse:response];
        [downloadProgressDelegate resource: identifier didReceiveResponse: response fromDataSource: dataSource];

        if (downloadError) {
            [self receivedError:downloadError forHandle:handle];
            [handle cancel];
        }
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
        
    if (downloadHandler) {
        [downloadHandler receivedData:data];
        [downloadProgressDelegate resource: identifier didReceiveContentLength: [data length] fromDataSource:dataSource];
    } else {
        [resourceData appendData:data];
        [dataSource _receivedData:data];
        [resourceProgressDelegate resource: identifier didReceiveContentLength: [data length] fromDataSource:dataSource];
        [[dataSource controller] _mainReceivedProgressForResourceHandle:handle
                                                             bytesSoFar:[resourceData length]
                                                         fromDataSource:dataSource
                                                               complete:NO];
    }
    
    LOG(Download, "%d of %d", [response contentLengthReceived], [response contentLength]);
}

- (void)handle:(WebResourceHandle *)handle didFailLoadingWithError:(WebError *)result
{
    LOG(Loading, "URL = %@, result = %@", [result failingURL], [result errorDescription]);

    if (!downloadHandler)
        [resourceProgressDelegate resource: identifier didFailLoadingWithError: result fromDataSource: dataSource];

    // Calling receivedError will likely result in a call to release, so we must retain.
    [self retain];

    [self receivedError:result forHandle:handle];

    if (downloadHandler) {
        [downloadHandler cancel];
        [downloadHandler release];
        downloadHandler = nil;
    }

    [identifier release];
    identifier = nil;
    
    [self didStopLoading];
    
    [self release];
}

@end
