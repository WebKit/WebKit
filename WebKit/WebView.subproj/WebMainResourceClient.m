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

@interface WebResourceDelegateProxy : NSObject <WebResourceDelegate>
{
    id <WebResourceDelegate> delegate;
}
- (void)setDelegate:(id <WebResourceDelegate>)theDelegate;
@end

@implementation WebMainResourceClient

- initWithDataSource:(WebDataSource *)ds
{
    self = [super init];
    
    if (self) {
        resourceData = [[NSMutableData alloc] init];
        [self setDataSource:ds];
        proxy = [[WebResourceDelegateProxy alloc] init];
        [proxy setDelegate:self];
    }

    return self;
}

- (void)dealloc
{    
    [resourceData release];
    [proxy release];
    
    [super dealloc];
}

- (NSData *)resourceData
{
    return resourceData;
}

- (void)receivedError:(WebError *)error complete:(BOOL)isComplete
{
    [dataSource _receivedError:error complete:isComplete];
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
	if (![WebContentTypes canShowMIMEType:[r contentType]]) {
	    [[dataSource webFrame] _handleUnimplementablePolicyWithErrorCode:WebKitErrorCannotShowMIMEType forURL:[req URL]];
	    [self stopLoadingForPolicyChange];
	    return;
	}
        break;
        
    case WebPolicySave:
        ASSERT([self downloadDelegate]);

        WebDownload *download = [[WebDownload alloc] _initWithLoadingResource:resource dataSource:dataSource];
        NSString *directory = [dataSource _downloadDirectory];
        if (directory != nil && [directory isAbsolutePath]) {
            // FIXME: Predetermined downloads should not be using this code path (3191052).
            NSString *path = [directory stringByAppendingPathComponent:[r suggestedFilenameForSaving]];
            [download _setPath:path];
        }

        [proxy setDelegate:(id <WebResourceDelegate>)download];
        [download release];
        
        [self interruptForPolicyChangeAndKeepLoading:YES];
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

    [super resource:resource didReceiveResponse:r];

    if ([[req URL] _web_shouldLoadAsEmptyDocument]) {
	[self resourceDidFinishLoading:resource];
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

    LOG(Loading, "main content type: %@", [r contentType]);

    [[dataSource _controller] setDefersCallbacks:YES];

    // FIXME: Predetermined downloads should not be using this code path (3191052).
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
    ASSERT(![[dataSource _controller] defersCallbacks]);
 
    LOG(Loading, "URL = %@, data = %p, length %d", [dataSource _URL], data, [data length]);

    [resourceData appendData:data];
    [dataSource _receivedData:data];
    [[dataSource _controller] _mainReceivedBytesSoFar:[resourceData length]
                                        fromDataSource:dataSource
                                            complete:NO];

    [super resource:h didReceiveData:data];
    _bytesReceived += [data length];

    LOG(Loading, "%d of %d", _bytesReceived, _contentLength);
}

- (void)resourceDidFinishLoading:(WebResource *)h
{
    ASSERT(![h defersCallbacks]);
    ASSERT(![self defersCallbacks]);
    ASSERT(![[dataSource _controller] defersCallbacks]);

    LOG(Loading, "URL = %@", [dataSource _URL]);
        
    // Calls in this method will most likely result in a call to release, so we must retain.
    [self retain];

    [dataSource _setResourceData:resourceData];
    [dataSource _finishedLoading];
    [[dataSource _controller] _mainReceivedBytesSoFar:[resourceData length]
                                        fromDataSource:dataSource
                                            complete:YES];
    [super resourceDidFinishLoading:h];
    
    [self release];
}

- (void)resource:(WebResource *)h didFailLoadingWithError:(WebError *)error
{
    ASSERT(![h defersCallbacks]);
    ASSERT(![self defersCallbacks]);
    ASSERT(![[dataSource _controller] defersCallbacks]);

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
	[self resource:resource willSendRequest:r];

	WebResponse *rsp = [[WebResponse alloc] init];
	[rsp setURL:[r URL]];
	[rsp setContentType:@"text/html"];
	[rsp setContentLength:0];
	[self resource:resource didReceiveResponse:rsp];
	[rsp release];
    } else {
	[resource loadWithDelegate:proxy];
    }
}

- (void)setDefersCallbacks:(BOOL)defers
{
    if (request && !([[request URL] _web_shouldLoadAsEmptyDocument])) {
	[super setDefersCallbacks:defers];
    }
}


@end

@implementation WebResourceDelegateProxy

- (void)setDelegate:(id <WebResourceDelegate>)theDelegate
{
    if (delegate != theDelegate) {
        [delegate release];
        delegate = [theDelegate retain];
    }
}

- (WebRequest *)resource:(WebResource *)resource willSendRequest:(WebRequest *)request
{
    return [delegate resource:resource willSendRequest:request];
}

-(void)resource:(WebResource *)resource didReceiveResponse:(WebResponse *)response
{
    [delegate resource:resource didReceiveResponse:response];
}

-(void)resource:(WebResource *)resource didReceiveData:(NSData *)data
{
    [delegate resource:resource didReceiveData:data];
}

-(void)resourceDidFinishLoading:(WebResource *)resource
{
    [delegate resourceDidFinishLoading:resource];
    [delegate release];
}

-(void)resource:(WebResource *)resource didFailLoadingWithError:(WebError *)error
{
    [delegate resource:resource didFailLoadingWithError:error];
    [delegate release];
}

@end
