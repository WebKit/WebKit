/*	
    WebMainResourceClient.m
    Copyright (c) 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebMainResourceClient.h>

#import <WebFoundation/NSHTTPCookie.h>
#import <WebFoundation/WebNSErrorExtras.h>

#import <WebFoundation/WebFileTypeMappings.h>
#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/NSURLConnection.h>
#import <WebFoundation/NSURLConnectionPrivate.h>
#import <WebFoundation/NSURLDownloadPrivate.h>
#import <WebFoundation/NSURLRequest.h>
#import <WebFoundation/NSURLRequestPrivate.h>
#import <WebFoundation/NSURLResponse.h>
#import <WebFoundation/NSURLResponsePrivate.h>

#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultPolicyDelegate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDownload.h>
#import <WebKit/WebFrameLoadDelegate.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebPolicyDelegatePrivate.h>
#import <WebKit/WebViewPrivate.h>

// FIXME: More that is in common with WebSubresourceClient should move up into WebBaseResourceHandleDelegate.

@implementation WebMainResourceClient

- initWithDataSource:(WebDataSource *)ds
{
    self = [super init];
    
    if (self) {
        [self setDataSource:ds];
        proxy = [[NSURLConnectionDelegateProxy alloc] init];
        [proxy setDelegate:self];
    }

    return self;
}

- (void)dealloc
{
    [proxy setDelegate:nil];
    [proxy release];
    
    [super dealloc];
}

- (void)receivedError:(NSError *)error
{
    // Calling _receivedError will likely result in a call to release, so we must retain.
    [self retain];
    [dataSource _receivedError:error complete:YES];
    [super connection:connection didFailWithError:error];
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

-(void)cancelWithError:(NSError *)error
{
    [self cancelContentPolicy];
    [connection cancel];
    [self receivedError:error];
}

- (NSError *)interruptForPolicyChangeError
{
    return [NSError _webKitErrorWithCode:WebKitErrorFrameLoadInterruptedByPolicyChange
                              failingURL:[[request URL] absoluteString]];
}

-(void)stopLoadingForPolicyChange
{
    [self retain];
    [self cancelWithError:[self interruptForPolicyChangeError]];
    [self release];
}

-(void)continueAfterNavigationPolicy:(NSURLRequest *)_request formState:(WebFormState *)state
{
    [[dataSource _webView] setDefersCallbacks:NO];
    if (!_request) {
	[self stopLoadingForPolicyChange];
    }
}

- (NSURLRequest *)connection:(NSURLConnection *)con willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse
{
    // Note that there are no asserts here as there are for the other callbacks. This is due to the
    // fact that this "callback" is sent when starting every load, and the state of callback
    // deferrals plays less of a part in this function in preventing the bad behavior deferring 
    // callbacks is meant to prevent.
    ASSERT(newRequest != nil);

    NSURL *URL = [newRequest URL];

    LOG(Redirect, "URL = %@", URL);

    NSMutableURLRequest *mutableRequest = nil;
    // Update cookie policy base URL as URL changes, except for subframes, which use the
    // URL of the main frame which doesn't change when we redirect.
    if ([dataSource webFrame] == [[dataSource _webView] mainFrame]) {
        mutableRequest = [newRequest mutableCopy];
        [mutableRequest setHTTPCookiePolicyBaseURL:URL];
    }

    // If we're fielding a redirect in response to a POST, force a load from origin, since
    // this is a common site technique to return to a page viewing some data that the POST
    // just modified.
    if (redirectResponse && [redirectResponse isKindOfClass:[NSHTTPURLResponse class]]) {
        int status = [(NSHTTPURLResponse *)redirectResponse statusCode];
        if (((status >= 301 && status <= 303) || status == 307)
            && [[[dataSource initialRequest] HTTPMethod] isEqualToString:@"POST"])
        {
            if (!mutableRequest) {
                mutableRequest = [newRequest mutableCopy];
            }
            [mutableRequest setCachePolicy:NSURLRequestReloadIgnoringCacheData];
        }
    }
    if (mutableRequest) {
        newRequest = [mutableRequest autorelease];
    }

    // note super will make a copy for us, so reassigning newRequest is important
    newRequest = [super connection:con willSendRequest:newRequest redirectResponse:redirectResponse];

    // Don't set this on the first request.  It is set
    // when the main load was started.
    [dataSource _setRequest:newRequest];
    
    [[dataSource _webView] setDefersCallbacks:YES];
    [[dataSource webFrame] _checkNavigationPolicyForRequest:newRequest
                                                 dataSource:dataSource
                                                  formState:nil
                                                    andCall:self
                                               withSelector:@selector(continueAfterNavigationPolicy:formState:)];

    return newRequest;
}

-(void)continueAfterContentPolicy:(WebPolicyAction)contentPolicy response:(NSURLResponse *)r
{
    [[dataSource _webView] setDefersCallbacks:NO];

    switch (contentPolicy) {
    case WebPolicyUse:
	if (![WebView canShowMIMEType:[r MIMEType]]) {
	    [[dataSource webFrame] _handleUnimplementablePolicyWithErrorCode:WebKitErrorCannotShowMIMEType forURL:[[dataSource request] URL]];
	    [self stopLoadingForPolicyChange];
	    return;
	}
        break;

    case WebPolicyDownload:
        [proxy setDelegate:nil];
        [WebDownload _downloadWithLoadingConnection:connection
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

    [self retain];

    [super connection:connection didReceiveResponse:r];

    if (![dataSource _isStopping]){
        if ([[request URL] _web_shouldLoadAsEmptyDocument]) {
            [self connectionDidFinishLoading:connection];
        }
    }
    
    [self release];
}

-(void)continueAfterContentPolicy:(WebPolicyAction)policy
{
    NSURLResponse *r = [policyResponse retain];
    BOOL isStopping = [dataSource _isStopping];

    [self cancelContentPolicy];
    if (!isStopping){
        [self continueAfterContentPolicy:policy response:r];
    }
    [r release];
}

-(void)checkContentPolicyForResponse:(NSURLResponse *)r
{
    listener = [[WebPolicyDecisionListener alloc]
		   _initWithTarget:self action:@selector(continueAfterContentPolicy:)];
    policyResponse = [r retain];

    WebView *wv = [dataSource _webView];
    [wv setDefersCallbacks:YES];
    [[wv _policyDelegateForwarder] webView:wv decidePolicyForMIMEType:[r MIMEType]
                                                            request:[dataSource request]
                                                              frame:[dataSource webFrame]
                                                   decisionListener:listener];
}


- (void)connection:(NSURLConnection *)con didReceiveResponse:(NSURLResponse *)r
{
    ASSERT(![con defersCallbacks]);
    ASSERT(![self defersCallbacks]);
    ASSERT(![[dataSource _webView] defersCallbacks]);

    LOG(Loading, "main content type: %@", [r MIMEType]);

    [dataSource _setResponse:r];
    _contentLength = [r expectedContentLength];

    [self checkContentPolicyForResponse:r];
}

- (void)connection:(NSURLConnection *)con didReceiveData:(NSData *)data
{
    ASSERT(data);
    ASSERT([data length] != 0);
    ASSERT(![connection defersCallbacks]);
    ASSERT(![self defersCallbacks]);
    ASSERT(![[dataSource _webView] defersCallbacks]);
 
    LOG(Loading, "URL = %@, data = %p, length %d", [dataSource _URL], data, [data length]);

    [dataSource _receivedData:data];
    [[dataSource _webView] _mainReceivedBytesSoFar:[[dataSource data] length]
                                       fromDataSource:dataSource
                                             complete:NO];

    [super connection:con didReceiveData:data];
    _bytesReceived += [data length];

    LOG(Loading, "%d of %d", _bytesReceived, _contentLength);
}

- (void)connectionDidFinishLoading:(NSURLConnection *)con
{
    ASSERT(![con defersCallbacks]);
    ASSERT(![self defersCallbacks]);
    ASSERT(![[dataSource _webView] defersCallbacks]);

    LOG(Loading, "URL = %@", [dataSource _URL]);
        
    // Calls in this method will most likely result in a call to release, so we must retain.
    [self retain];

    [dataSource _finishedLoading];
    [[dataSource _webView] _mainReceivedBytesSoFar:[[dataSource data] length]
                                    fromDataSource:dataSource
                                            complete:YES];
    [super connectionDidFinishLoading:con];

    [self release];
}

- (void)connection:(NSURLConnection *)con didFailWithError:(NSError *)error
{
    ASSERT(![con defersCallbacks]);
    ASSERT(![self defersCallbacks]);
    ASSERT(![[dataSource _webView] defersCallbacks]);

    LOG(Loading, "URL = %@, error = %@", [error _web_failingURL], [error _web_localizedDescription]);

    [self receivedError:error];
}

- (BOOL)loadWithRequest:(NSURLRequest *)r
{
    ASSERT(connection == nil);
    
    if ([[r URL] _web_shouldLoadAsEmptyDocument]) {
        connection = [[NSURLConnection alloc] initWithRequest:r delegate:nil];

	[self connection:connection willSendRequest:r redirectResponse:nil];

	NSURLResponse *rsp = [[NSURLResponse alloc] initWithURL:[[[self dataSource] request] URL]
						    MIMEType:@"text/html"
						    expectedContentLength:0
						    textEncodingName:nil];
	[self connection:connection didReceiveResponse:rsp];
	[rsp release];
    } else {
        // send this synthetic delegate callback since clients expect it, and
        // we no longer send the callback from within NSURLConnection for
        // initial requests.
        r = [proxy connection:nil willSendRequest:r redirectResponse:nil];
        connection = [[NSURLConnection alloc] initWithRequest:r delegate:proxy];
        if ([self defersCallbacks]) {
            [connection setDefersCallbacks:YES];
        }
    }

    return YES;
}

- (void)setDefersCallbacks:(BOOL)defers
{
    if (request && !([[request URL] _web_shouldLoadAsEmptyDocument])) {
	[super setDefersCallbacks:defers];
    }
}


@end

