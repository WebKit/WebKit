/*	
    WebMainResourceClient.m
    Copyright (c) 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebMainResourceClient.h>

#import <WebFoundation/WebCookieConstants.h>
#import <WebFoundation/WebError.h>
#import <WebFoundation/WebFileTypeMappings.h>
#import <WebFoundation/WebResourceHandle.h>
#import <WebFoundation/WebResourceRequest.h>
#import <WebFoundation/WebHTTPResourceRequest.h>
#import <WebFoundation/WebResourceResponse.h>

#import <WebKit/WebBridge.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebControllerPolicyDelegatePrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDownloadHandler.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebLocationChangeDelegate.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebStandardPanelsPrivate.h>
#import <WebKit/WebView.h>

// FIXME: More that is in common with WebSubresourceClient should move up into WebBaseResourceHandleDelegate.

@implementation WebMainResourceClient

- initWithDataSource:(WebDataSource *)ds
{
    self = [super init];
    
    if (self) {
        resourceData = [[NSMutableData alloc] init];
        [self setDataSource:ds];
    }

    return self;
}

- (void)dealloc
{
    ASSERT(downloadHandler == nil);
    
    [resourceData release];
    
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

- (void)receivedError:(WebError *)error
{    
    WebContentAction contentAction = [[dataSource contentPolicy] policyAction];

    if (contentAction != WebContentPolicySave) {
        [[dataSource controller] _mainReceivedError:error fromDataSource:dataSource];
    }
}

- (void)cancel
{
    LOG(Loading, "URL = %@", [dataSource URL]);
    
    // Calling receivedError will likely result in a call to release, so we must retain.
    [self retain];
    
    [self receivedError:[self cancelledError]];

    if (downloadHandler) {
        [downloadHandler cancel];
        [downloadHandler release];
        downloadHandler = nil;
    }
        
    [super cancel];

    [self release];
}

-(WebResourceRequest *)handle:(WebResourceHandle *)h willSendRequest:(WebResourceRequest *)newRequest
{
    newRequest = [super handle:h willSendRequest:newRequest];
    
    ASSERT(newRequest != nil);

    NSURL *URL = [newRequest URL];

    if (![[dataSource webFrame] _shouldShowRequest:newRequest]) {
        [self cancelQuietly];

        [[dataSource webFrame] _setProvisionalDataSource:nil];
	[[[dataSource controller] locationChangeDelegate] locationChangeDone:
            [WebError errorWithCode:WebErrorLocationChangeInterruptedByPolicyChange inDomain:WebErrorDomainWebKit failingURL:nil]
            forDataSource:dataSource];
        return nil;
    }
    
    LOG(Redirect, "URL = %@", URL);
    
    // Update cookie policy base URL as URL changes, except for subframes, which use the
    // URL of the main frame which doesn't change when we redirect.
    if ([dataSource webFrame] == [[dataSource controller] mainFrame]) {
        [newRequest setCookiePolicyBaseURL:URL];
    }
    
    // Don't set this on the first request.  It is set
    // when the main load was started.
    [dataSource _setRequest:newRequest];
    
    return newRequest;
}

- (void)notifyDelegatesOfInterruptionByPolicyChange
{
    // Terminate the locationChangeDelegate correctly.
    WebError *interruptError = [WebError errorWithCode:WebErrorLocationChangeInterruptedByPolicyChange inDomain:WebErrorDomainWebKit failingURL:nil];
    [[[dataSource controller] locationChangeDelegate] locationChangeDone:interruptError forDataSource:dataSource];

    [super notifyDelegatesOfInterruptionByPolicyChange];
}

-(void)handle:(WebResourceHandle *)h didReceiveResponse:(WebResourceResponse *)r
{
    [dataSource _setResponse:r];

    LOG(Download, "main content type: %@", [r contentType]);

    // Figure out the content policy.
    WebContentPolicy *contentPolicy = [dataSource contentPolicy];

    if ([contentPolicy policyAction] != WebPolicySave) {
	contentPolicy = [[[dataSource controller] policyDelegate] contentPolicyForResponse:r
								  andRequest:[dataSource request]
								  inFrame:[dataSource webFrame]];
    }

    if ([contentPolicy policyAction] == WebPolicySave) {
	NSString *saveFilename = [[[dataSource controller] policyDelegate] saveFilenameForResponse:r andRequest:[dataSource request]];
	[contentPolicy _setPath:saveFilename];
    }

    [dataSource _setContentPolicy:contentPolicy];

    policyAction = [contentPolicy policyAction];

    switch (policyAction) {
    case WebContentPolicyShow:
        break;
        
    case WebContentPolicySave:
        {
            [[dataSource webFrame] _setProvisionalDataSource:nil];
            
            [self notifyDelegatesOfInterruptionByPolicyChange];
            
            // Hand off the dataSource to the download handler.  This will cause the remaining
            // handle delegate callbacks to go to the controller's download delegate.
            downloadHandler = [[WebDownloadHandler alloc] initWithDataSource:dataSource];
            [self setIsDownload:YES];
        }
        break;
    
    case WebContentPolicyIgnore:
        {
            [self cancel];
            [[dataSource webFrame] _setProvisionalDataSource:nil];
            [self notifyDelegatesOfInterruptionByPolicyChange];
        }
        break;
    
    default:
        ERROR("contentPolicyForMIMEType:URL:inFrame: returned an invalid content policy.");
    }

    [super handle:h didReceiveResponse:r];
    
    _contentLength = [r contentLength];
}

- (void)handle:(WebResourceHandle *)h didReceiveData:(NSData *)data
{
    LOG(Loading, "URL = %@, data = %p, length %d", [dataSource URL], data, [data length]);

    if (downloadHandler) {
        WebError *downloadError = [downloadHandler receivedData:data];
        if (downloadError) {
            [self receivedError:downloadError];
            [self cancel];
            return;
        }
    } else {
        [resourceData appendData:data];
        [dataSource _receivedData:data];
        [[dataSource controller] _mainReceivedBytesSoFar:[resourceData length]
                                          fromDataSource:dataSource
                                                complete:NO];
    }
    
    [super handle:h didReceiveData:data];
    
    _bytesReceived += [data length];

    LOG(Download, "%d of %d", _bytesReceived, _contentLength);
}

- (void)handleDidFinishLoading:(WebResourceHandle *)h
{
    LOG(Loading, "URL = %@", [dataSource URL]);
        
    // Calling receivedError will likely result in a call to release, so we must retain.
    [self retain];

    WebContentAction contentAction = [[dataSource contentPolicy] policyAction];
    
    // Don't retain data for downloaded files
    if (contentAction != WebContentPolicySave) {
    	[dataSource _setResourceData:resourceData];
    }

    if (contentAction == WebContentPolicyShow) {
        [[dataSource representation] finishedLoadingWithDataSource:dataSource];
    }
    
    if (downloadHandler) {
        WebError *downloadError = [downloadHandler finishedLoading];
        if (downloadError)
            [self receivedError:downloadError];

        [dataSource _setPrimaryLoadComplete:YES];
        [downloadHandler release];
        downloadHandler = nil;
    }
    else {
        [dataSource _finishedLoading];
        [[dataSource controller] _mainReceivedBytesSoFar:[resourceData length]
                                          fromDataSource:dataSource
                                                complete:YES];
    }
    
    [super handleDidFinishLoading:h];

    [self release];
}

- (void)handle:(WebResourceHandle *)h didFailLoadingWithError:(WebError *)result
{
    LOG(Loading, "URL = %@, result = %@", [result failingURL], [result errorDescription]);

    // Calling receivedError will likely result in a call to release, so we must retain.
    [self retain];

    [self receivedError:result];

    if (downloadHandler) {
        [downloadHandler cancel];
        [downloadHandler release];
        downloadHandler = nil;
    }
    
    [super handle:h didFailLoadingWithError:result];

    [self release];
}

@end
