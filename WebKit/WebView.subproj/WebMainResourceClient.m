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
#import <WebKit/WebControllerPolicyDelegatePrivate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDownloadPrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebLocationChangeDelegate.h>
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
        proxy = [[WebResourceDelegateProxy alloc] init];
        [proxy setDelegate:self];
    }

    return self;
}

- (void)dealloc
{
    [resourceData release];
    [proxy setDelegate:nil];
    [proxy release];
    
    [super dealloc];
}

- (NSData *)resourceData
{
    return resourceData;
}

- (void)receivedError:(WebError *)error
{
    // Calling _receivedError will likely result in a call to release, so we must retain.
    [self retain];
    [dataSource _receivedError:error complete:YES];
    [super resource:resource didFailLoadingWithError:error];
    [self release];
}

- (void)cancelContentPolicy
{
    [listener _invalidate];
    [listener release];
    listener = nil;
    [policyResponse release];
    policyResponse = nil;
}

-(void)cancelWithError:(WebError *)error
{
    [self cancelContentPolicy];
    [resource cancel];
    [self receivedError:error];
}

- (WebError *)interruptForPolicyChangeError
{
    return [WebError errorWithCode:WebKitErrorLocationChangeInterruptedByPolicyChange
                          inDomain:WebErrorDomainWebKit
                        failingURL:nil];
}

-(void)stopLoadingForPolicyChange
{
    [self cancelWithError:[self interruptForPolicyChangeError]];
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
    
    [[dataSource _controller] setDefersCallbacks:YES];
    [[dataSource webFrame] _checkNavigationPolicyForRequest:newRequest
                                                 dataSource:dataSource
                                                  formState:nil
                                                    andCall:self
                                               withSelector:@selector(continueAfterNavigationPolicy:formState:)];

    return newRequest;
}

-(void)continueAfterContentPolicy:(WebPolicyAction)contentPolicy response:(WebResponse *)r
{
    [[dataSource _controller] setDefersCallbacks:NO];
    WebRequest *req = [dataSource request];

    switch (contentPolicy) {
    case WebPolicyUse:
	if (![WebContentTypes canShowMIMEType:[r contentType]]) {
	    [[dataSource webFrame] _handleUnimplementablePolicyWithErrorCode:WebKitErrorCannotShowMIMEType forURL:[req URL]];
	    [self stopLoadingForPolicyChange];
	    return;
	}
        break;

    case WebPolicyDownload:
        [proxy setDelegate:nil];
        [WebDownload _downloadWithLoadingResource:resource
                                          request:request
                                         response:r
                                         delegate:[self downloadDelegate]
                                            proxy:proxy];
        [proxy release];
        proxy = nil;
        [self receivedError:[self interruptForPolicyChangeError]];
        return;

    case WebPolicyIgnore:
	[self stopLoadingForPolicyChange];
	return;
    
    default:
	ASSERT_NOT_REACHED();
    }

    [super resource:resource didReceiveResponse:r];

    if ([[req URL] _web_shouldLoadAsEmptyDocument]) {
	[self resourceDidFinishLoading:resource];
    }
}

-(void)continueAfterContentPolicy:(WebPolicyAction)policy
{
    WebResponse *r = [policyResponse retain];
    [self cancelContentPolicy];
    [self continueAfterContentPolicy:policy response:r];
    [r release];
}

-(void)checkContentPolicyForResponse:(WebResponse *)r
{
    listener = [[WebPolicyDecisionListener alloc]
		   _initWithTarget:self action:@selector(continueAfterContentPolicy:)];
    policyResponse = [r retain];

    WebController *c = [dataSource _controller];
    [c setDefersCallbacks:YES];
    [[c _policyDelegateForwarder] controller:c decideContentPolicyForMIMEType:[r contentType]
                                                                   andRequest:[dataSource request]
                                                                      inFrame:[dataSource webFrame]
                                                             decisionListener:listener];
}


-(void)resource:(WebResource *)h didReceiveResponse:(WebResponse *)r
{
    ASSERT(![h defersCallbacks]);
    ASSERT(![self defersCallbacks]);
    ASSERT(![[dataSource _controller] defersCallbacks]);

    LOG(Loading, "main content type: %@", [r contentType]);

    [dataSource _setResponse:r];
    _contentLength = [r contentLength];

    // Figure out the content policy.
    [self checkContentPolicyForResponse:r];
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

    [self receivedError:error];
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
    delegate = theDelegate;
}

- (WebRequest *)resource:(WebResource *)resource willSendRequest:(WebRequest *)request
{
    ASSERT(delegate);
    return [delegate resource:resource willSendRequest:request];
}

-(void)resource:(WebResource *)resource didReceiveResponse:(WebResponse *)response
{
    ASSERT(delegate);
    [delegate resource:resource didReceiveResponse:response];
}

-(void)resource:(WebResource *)resource didReceiveData:(NSData *)data
{
    ASSERT(delegate);
    [delegate resource:resource didReceiveData:data];
}

-(void)resourceDidFinishLoading:(WebResource *)resource
{
    ASSERT(delegate);
    [delegate resourceDidFinishLoading:resource];
}

-(void)resource:(WebResource *)resource didFailLoadingWithError:(WebError *)error
{
    ASSERT(delegate);
    [delegate resource:resource didFailLoadingWithError:error];
}

@end
