/*	
    WebMainResourceClient.m
    Copyright (c) 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebMainResourceClient.h>

#import <WebFoundation/WebCookieConstants.h>
#import <WebFoundation/WebError.h>
#import <WebFoundation/WebHTTPRequest.h>
#import <WebFoundation/WebFileTypeMappings.h>
#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebResource.h>
#import <WebFoundation/WebRequest.h>
#import <WebFoundation/WebResponse.h>
#import <WebFoundation/WebMutableResponse.h>

#import <WebKit/WebController.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultPolicyDelegate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDownloadPrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebLocationChangeDelegate.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebResourceResponseExtras.h>
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
    ASSERT(download == nil);
    
    [resourceData release];
    
    [super dealloc];
}

- (NSData *)resourceData
{
    return resourceData;
}

- (WebDownload *)download
{
    return download;
}

- (BOOL)isDownload
{
    return download != nil;
}

- (void)receivedError:(WebError *)error complete:(BOOL)isComplete
{
    if (download) {
        ASSERT(isComplete);
        [download cancel];
        [download release];
        download = nil;
        [dataSource _setPrimaryLoadComplete:YES];
    } else {
        [dataSource _receivedError:error complete:isComplete];
    }
}

- (void)cancel
{
    LOG(Loading, "URL = %@", [dataSource _URL]);
    
    // Calling receivedError will likely result in a call to release, so we must retain.
    [self retain];

    [self receivedError:[self cancelledError] complete:YES];
    [super cancel];

    [self release];
}

- (void)interruptForPolicyChangeAndKeepLoading:(BOOL)keepLoading
{
    // Terminate the locationChangeDelegate correctly.
    WebError *interruptError = [WebError errorWithCode:WebKitErrorLocationChangeInterruptedByPolicyChange inDomain:WebErrorDomainWebKit failingURL:nil];

    // Must call receivedError before _clearProvisionalDataSource because
    // if we remove the data source from the frame, we can't get back to the frame any more.
    [self receivedError:interruptError complete:!keepLoading];

    [[dataSource webFrame] _clearProvisionalDataSource];
    
    // Deliver the error to the location change delegate.
    // We have to do this explicitly because since we are still loading, WebFrame
    // won't do it for us. Also, we have to do this after the provisional data source
    // is cleared so the delegate will get false if they ask the frame if it's loading.
    // There's probably a better way to do this, but this should do for now.
    if (keepLoading) {
        [[[dataSource _controller] _locationChangeDelegateForwarder]
            locationChangeDone:interruptError forDataSource:dataSource];
    }
	
    [self notifyDelegatesOfInterruptionByPolicyChange];
}

-(void)stopLoadingForPolicyChange
{
    [self interruptForPolicyChangeAndKeepLoading:NO];
    [self cancelQuietly];
}

-(void)continueAfterNavigationPolicy:(WebRequest *)_request formState:(WebFormState *)state
{
    [[dataSource _controller] setDefersCallbacks:NO];
    if (!_request) {
	[self stopLoadingForPolicyChange];
    }
}

-(WebRequest *)resource:(WebResource *)h willSendRequest:(WebRequest *)newRequest
{
    // Note that there are no asserts here as there are for the other callbacks. This is due to the
    // fact that this "callback" is sent when starting every load, and the state of callback
    // deferrals plays less of a part in this function in preventing the bad behavior deferring 
    // callbacks is meant to prevent.
    ASSERT(newRequest != nil);

    NSURL *URL = [newRequest URL];

    LOG(Redirect, "URL = %@", URL);
    
    // Update cookie policy base URL as URL changes, except for subframes, which use the
    // URL of the main frame which doesn't change when we redirect.
    if ([dataSource webFrame] == [[dataSource _controller] mainFrame]) {
        [newRequest setCookiePolicyBaseURL:URL];
    }

    // note super will make a copy for us, so reassigning newRequest is important
    newRequest = [super resource:h willSendRequest:newRequest];

    // Don't set this on the first request.  It is set
    // when the main load was started.
    [dataSource _setRequest:newRequest];
    [[dataSource webFrame] _checkNavigationPolicyForRequest:newRequest dataSource:dataSource formState:nil andCall:self withSelector:@selector(continueAfterNavigationPolicy:formState:)];

    return newRequest;
}

-(void)continueAfterContentPolicy:(WebPolicyAction)contentPolicy response:(WebResponse *)r
{
    [[dataSource _controller] setDefersCallbacks:NO];
    WebRequest *req = [dataSource request];

    switch (contentPolicy) {
    case WebPolicyShow:
	if (![WebCapabilities canShowMIMEType:[r contentType]]) {
	    [[dataSource webFrame] _handleUnimplementablePolicyWithErrorCode:WebKitErrorCannotShowMIMEType forURL:[req URL]];
	    [self stopLoadingForPolicyChange];
	    return;
	}
        break;
        
    case WebPolicySave:
	[dataSource _setIsDownloading:YES];

        NSString *path = [dataSource downloadPath];
        if (path == nil || ![path isAbsolutePath]) {
            NSString *directory = [dataSource _downloadDirectory];
            if (directory != nil && [directory isAbsolutePath]) {
                path = [directory stringByAppendingPathComponent:[r suggestedFilenameForSaving]];
            } else {
                id pd = [[dataSource _controller] policyDelegate];
                
                if ([pd respondsToSelector: @selector(savePathForResponse:andRequest:)])
                    path = [pd savePathForResponse:r andRequest:req];
                // FIXME: Maybe there a cleaner way handle the bad filename case?
                if (path == nil || ![path isAbsolutePath]) {
                    ERROR("Nil or non-absolute path returned from savePathForResponse:andRequest:.");
                    [self stopLoadingForPolicyChange];
                    return;
                }
            }

	    [dataSource _setDownloadPath:path];
	}

        [self interruptForPolicyChangeAndKeepLoading:YES];
        
	// Hand off the dataSource to the download handler.  This will cause the remaining
	// handle delegate callbacks to go to the controller's download delegate.
	download = [[WebDownload alloc] initWithDataSource:dataSource];
        break;

    case WebPolicyOpenURL:
	if ([[req URL] isFileURL]) {
	    if(![[NSWorkspace sharedWorkspace] openFile:[[req URL] path]]){
		[[dataSource webFrame] _handleUnimplementablePolicyWithErrorCode:WebKitErrorCannotFindApplicationForFile forURL:[req URL]];
	    }
	} else {
	    if(![[NSWorkspace sharedWorkspace] openURL:[req URL]]){
		[[dataSource webFrame] _handleUnimplementablePolicyWithErrorCode:WebKitErrorCannotFindApplicationForURL forURL:[req URL]];
	    }
	}

	[self stopLoadingForPolicyChange];
	return;
	break;
	
    case WebPolicyRevealInFinder:
	if (![[req URL] isFileURL]) {
	    ERROR("contentPolicyForMIMEType:andRequest:inFrame: returned an invalid content policy.");
	} else if (![[NSWorkspace sharedWorkspace] selectFile:[[req URL] path] inFileViewerRootedAtPath:@""]) {
	    [[dataSource webFrame] _handleUnimplementablePolicyWithErrorCode:WebKitErrorFinderCannotOpenDirectory forURL:[req URL]];
	}

	[self stopLoadingForPolicyChange];
	return;
	break;
    
    case WebPolicyIgnore:
	[self stopLoadingForPolicyChange];
	return;
        break;
    
    default:
        ERROR("contentPolicyForMIMEType:andRequest:inFrame: returned an invalid content policy.");
    }

    [super resource:handle didReceiveResponse:r];

    if ([[req URL] _web_shouldLoadAsEmptyDocument]) {
	[self resourceDidFinishLoading:handle];
    }
}


-(void)checkContentPolicyForResponse:(WebResponse *)r andCallSelector:(SEL)selector
{
    id pd = [[dataSource _controller] policyDelegate];
    WebPolicyAction contentPolicy;
    
    if ([pd respondsToSelector:@selector(contentPolicyForMIMEType:andRequest:inFrame:)])
        contentPolicy = [pd contentPolicyForMIMEType:[r contentType]
                                                                andRequest:[dataSource request]
                                                                   inFrame:[dataSource webFrame]];
    else
        contentPolicy = [[WebDefaultPolicyDelegate sharedPolicyDelegate] contentPolicyForMIMEType:[r contentType]
                                                                andRequest:[dataSource request]
                                                                   inFrame:[dataSource webFrame]];
    [self performSelector:selector withObject:(id)contentPolicy withObject:r];
}


-(void)resource:(WebResource *)h didReceiveResponse:(WebResponse *)r
{
    ASSERT(![h defersCallbacks]);
    ASSERT(![self defersCallbacks]);
    ASSERT([dataSource isDownloading] || ![[dataSource _controller] defersCallbacks]);
    [dataSource _setResponse:r];

    LOG(Download, "main content type: %@", [r contentType]);

    [[dataSource _controller] setDefersCallbacks:YES];

    // Figure out the content policy.
    if (![dataSource isDownloading]) {
	[self checkContentPolicyForResponse:r andCallSelector:@selector(continueAfterContentPolicy:response:)];
    } else {
	[self continueAfterContentPolicy:WebPolicySave response:r];
    }

    _contentLength = [r contentLength];
}

- (void)resource:(WebResource *)h didReceiveData:(NSData *)data
{
    ASSERT(data);
    ASSERT([data length] != 0);
    ASSERT(![h defersCallbacks]);
    ASSERT(![self defersCallbacks]);
    ASSERT([self isDownload] || ![[dataSource _controller] defersCallbacks]);
 
    LOG(Loading, "URL = %@, data = %p, length %d", [dataSource _URL], data, [data length]);

    WebError *downloadError= nil;
    
    if (download) {
        downloadError = [download receivedData:data];
    } else {
        [resourceData appendData:data];
        [dataSource _receivedData:data];
        [[dataSource _controller] _mainReceivedBytesSoFar:[resourceData length]
                                          fromDataSource:dataSource
                                                complete:NO];
    }
    
    [super resource:h didReceiveData:data];
    _bytesReceived += [data length];
    
    if(downloadError){
        // Cancel download after calling didReceiveData to preserve ordering of calls.
        [self cancelWithError:downloadError];
    }

    LOG(Download, "%d of %d", _bytesReceived, _contentLength);
}

- (void)resourceDidFinishLoading:(WebResource *)h
{
    ASSERT(![h defersCallbacks]);
    ASSERT(![self defersCallbacks]);
    ASSERT([self isDownload] || ![[dataSource _controller] defersCallbacks]);
    LOG(Loading, "URL = %@", [dataSource _URL]);
        
    // Calls in this method will most likely result in a call to release, so we must retain.
    [self retain];

    WebError *downloadError = nil;
    
    if (download) {
        downloadError = [download finishedLoading];
        [dataSource _setPrimaryLoadComplete:YES];
    } else {
        [dataSource _setResourceData:resourceData];
        [dataSource _finishedLoading];
        [[dataSource _controller] _mainReceivedBytesSoFar:[resourceData length]
                                          fromDataSource:dataSource
                                                complete:YES];
    }

    if(downloadError){
        [super resource:h didFailLoadingWithError:downloadError];
    } else {
        [super resourceDidFinishLoading:h];
    }

    [download release];
    download = nil;
    
    [self release];
}

- (void)resource:(WebResource *)h didFailLoadingWithError:(WebError *)error
{
    ASSERT(![h defersCallbacks]);
    ASSERT(![self defersCallbacks]);
    ASSERT([self isDownload] || ![[dataSource _controller] defersCallbacks]);
    LOG(Loading, "URL = %@, error = %@", [error failingURL], [error errorDescription]);

    // Calling receivedError will likely result in a call to release, so we must retain.
    [self retain];

    [self receivedError:error complete:YES];
    [super resource:h didFailLoadingWithError:error];

    [self release];
}

- (void)startLoading:(WebRequest *)r
{
    if ([[r URL] _web_shouldLoadAsEmptyDocument]) {
	[self resource:handle willSendRequest:r];

	WebResponse *rsp = [[WebResponse alloc] init];
	[rsp setURL:[r URL]];
	[rsp setContentType:@"text/html"];
	[rsp setContentLength:0];
	[self resource:handle didReceiveResponse:rsp];
	[rsp release];
    } else {
	[super startLoading:r];
    }
}

- (void)setDefersCallbacks:(BOOL)defers
{
    if (request && !([[request URL] _web_shouldLoadAsEmptyDocument])) {
	[super setDefersCallbacks:defers];
    }
}


@end
