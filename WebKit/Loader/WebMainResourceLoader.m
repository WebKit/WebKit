/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/WebMainResourceLoader.h>

#import <Foundation/NSHTTPCookie.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLResponse.h>
#import <JavaScriptCore/Assertions.h>

#import <WebKit/WebDataProtocol.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebFrameLoader.h>

// FIXME: More that is in common with WebSubresourceLoader should move up into WebLoader.

@implementation WebMainResourceLoader

- (id)initWithFrameLoader:(WebFrameLoader *)fl
{
    self = [super init];
    
    if (self) {
        [self setFrameLoader:fl];
        proxy = WKCreateNSURLConnectionDelegateProxy();
        [proxy setDelegate:self];
    }

    return self;
}

- (void)dealloc
{
    [_initialRequest release];

    [proxy setDelegate:nil];
    [proxy release];
    
    [super dealloc];
}

- (void)finalize
{
    [proxy setDelegate:nil];
    [super finalize];
}

- (void)receivedError:(NSError *)error
{
    // Calling _receivedMainResourceError will likely result in a call to release, so we must retain.
    [self retain];
    WebFrameLoader *fl = [frameLoader retain]; // super's didFailWithError will release the frameLoader

    if (!cancelledFlag) {
        ASSERT(!reachedTerminalState);
        [frameLoader _didFailLoadingWithError:error forResource:identifier];
    }

    [fl _receivedMainResourceError:error complete:YES];

    if (!cancelledFlag)
        [self releaseResources];

    ASSERT(reachedTerminalState);

    [fl release];
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
    // Calling _receivedMainResourceError will likely result in a call to release, so we must retain.
    [self retain];

    [self cancelContentPolicy];
    [frameLoader retain];
    [frameLoader _receivedMainResourceError:error complete:YES];
    [frameLoader release];
    [super cancelWithError:error];

    [self release];
}

- (NSError *)interruptForPolicyChangeError
{
    return [frameLoader interruptForPolicyChangeErrorWithRequest:request];
}

-(void)stopLoadingForPolicyChange
{
    [self retain];
    [self cancelWithError:[self interruptForPolicyChangeError]];
    [self release];
}

-(void)continueAfterNavigationPolicy:(NSURLRequest *)_request formState:(WebFormState *)state
{
    if (!_request) {
        [self stopLoadingForPolicyChange];
    }
}

- (BOOL)_isPostOrRedirectAfterPost:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse
{
    BOOL result = NO;
    
    if ([[newRequest HTTPMethod] isEqualToString:@"POST"]) {
        result = YES;
    }
    else if (redirectResponse && [redirectResponse isKindOfClass:[NSHTTPURLResponse class]]) {
        int status = [(NSHTTPURLResponse *)redirectResponse statusCode];
        if (((status >= 301 && status <= 303) || status == 307)
            && [[[frameLoader initialRequest] HTTPMethod] isEqualToString:@"POST"]) {
            result = YES;
        }
    }
    
    return result;
}

- (void)addData:(NSData *)data allAtOnce:(BOOL)allAtOnce
{
    [super addData:data allAtOnce:allAtOnce];
    [frameLoader _receivedData:data];
}

- (void)saveResource
{
    // Override. We don't want to save the main resource as a subresource of the data source.
}

- (NSURLRequest *)willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse
{
    // Note that there are no asserts here as there are for the other callbacks. This is due to the
    // fact that this "callback" is sent when starting every load, and the state of callback
    // deferrals plays less of a part in this function in preventing the bad behavior deferring 
    // callbacks is meant to prevent.
    ASSERT(newRequest != nil);

    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];

    NSURL *URL = [newRequest URL];

    NSMutableURLRequest *mutableRequest = nil;
    // Update cookie policy base URL as URL changes, except for subframes, which use the
    // URL of the main frame which doesn't change when we redirect.
    if ([frameLoader isLoadingMainFrame]) {
        mutableRequest = [newRequest mutableCopy];
        [mutableRequest setMainDocumentURL:URL];
    }

    // If we're fielding a redirect in response to a POST, force a load from origin, since
    // this is a common site technique to return to a page viewing some data that the POST
    // just modified.
    // Also, POST requests always load from origin, but this does not affect subresources.
    if ([newRequest cachePolicy] == NSURLRequestUseProtocolCachePolicy && 
        [self _isPostOrRedirectAfterPost:newRequest redirectResponse:redirectResponse]) {
        if (!mutableRequest) {
            mutableRequest = [newRequest mutableCopy];
        }
        [mutableRequest setCachePolicy:NSURLRequestReloadIgnoringCacheData];
    }
    if (mutableRequest) {
        newRequest = [mutableRequest autorelease];
    }

    // Note super will make a copy for us, so reassigning newRequest is important. Since we are returning this value, but
    // it's only guaranteed to be retained by self, and self might be dealloc'ed in this method, we have to autorelease.
    // See 3777253 for an example.
    newRequest = [[[super willSendRequest:newRequest redirectResponse:redirectResponse] retain] autorelease];

    // Don't set this on the first request.  It is set
    // when the main load was started.
    [frameLoader _setRequest:newRequest];
    
    [[frameLoader webFrame] _checkNavigationPolicyForRequest:newRequest
                                                  dataSource:[frameLoader activeDataSource]
                                                   formState:nil
                                                     andCall:self
                                                withSelector:@selector(continueAfterNavigationPolicy:formState:)];

    [self release];
    return newRequest;
}

static BOOL isCaseInsensitiveEqual(NSString *a, NSString *b)
{
    return [a caseInsensitiveCompare:b] == NSOrderedSame;
}

#define URL_BYTES_BUFFER_LENGTH 2048

static BOOL shouldLoadAsEmptyDocument(NSURL *url)
{
    UInt8 *buffer = (UInt8 *)malloc(URL_BYTES_BUFFER_LENGTH);
    CFIndex bytesFilled = CFURLGetBytes((CFURLRef)url, buffer, URL_BYTES_BUFFER_LENGTH);
    if (bytesFilled == -1) {
        CFIndex bytesToAllocate = CFURLGetBytes((CFURLRef)url, NULL, 0);
        buffer = (UInt8 *)realloc(buffer, bytesToAllocate);
        bytesFilled = CFURLGetBytes((CFURLRef)url, buffer, bytesToAllocate);
        ASSERT(bytesFilled == bytesToAllocate);
    }
    
    BOOL result = (bytesFilled == 0) || (bytesFilled > 5 && strncmp((char *)buffer, "about:", 6) == 0);
    free(buffer);

    return result;
}

-(void)continueAfterContentPolicy:(WebPolicyAction)contentPolicy response:(NSURLResponse *)r
{
    NSURL *URL = [request URL];
    NSString *MIMEType = [r MIMEType]; 
    
    switch (contentPolicy) {
    case WebPolicyUse:
    {
        // Prevent remote web archives from loading because they can claim to be from any domain and thus avoid cross-domain security checks (4120255).
        BOOL isRemote = ![URL isFileURL] && ![WebDataProtocol _webIsDataProtocolURL:URL];
        BOOL isRemoteWebArchive = isRemote && isCaseInsensitiveEqual(@"application/x-webarchive", MIMEType);
        if (![WebFrameLoader _canShowMIMEType:MIMEType] || isRemoteWebArchive) {
            [frameLoader cannotShowMIMETypeForURL:URL];
            // Check reachedTerminalState since the load may have already been cancelled inside of _handleUnimplementablePolicyWithErrorCode::.
            if (!reachedTerminalState) {
                [self stopLoadingForPolicyChange];
            }
            return;
        }
        break;
    }
    case WebPolicyDownload:
        [proxy setDelegate:nil];
        [frameLoader _downloadWithLoadingConnection:connection request:request response:r proxy:proxy];
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

    if ([r isKindOfClass:[NSHTTPURLResponse class]]) {
        int status = [(NSHTTPURLResponse *)r statusCode];
        if (status < 200 || status >= 300) {
            BOOL hostedByObject = [frameLoader isHostedByObjectElement];

            [frameLoader _handleFallbackContent];
            // object elements are no longer rendered after we fallback, so don't
            // keep trying to process data from their load

            if (hostedByObject)
                [self cancel];
        }
    }

    // we may have cancelled this load as part of switching to fallback content
    if (!reachedTerminalState) {
        [super didReceiveResponse:r];
    }

    if (![frameLoader _isStopping] && (shouldLoadAsEmptyDocument(URL) || [WebFrameLoader _representationExistsForURLScheme:[URL scheme]])) {
        [self didFinishLoading];
    }
    
    [self release];
}

-(void)continueAfterContentPolicy:(WebPolicyAction)policy
{
    NSURLResponse *r = [policyResponse retain];
    BOOL isStopping = [frameLoader _isStopping];

    [self cancelContentPolicy];
    if (!isStopping)
        [self continueAfterContentPolicy:policy response:r];

    [r release];
}

-(void)checkContentPolicyForResponse:(NSURLResponse *)r
{
    WebPolicyDecisionListener *l = [[WebPolicyDecisionListener alloc]
                                       _initWithTarget:self action:@selector(continueAfterContentPolicy:)];
    listener = l;
    policyResponse = [r retain];

    [l retain];
    [frameLoader _decidePolicyForMIMEType:[r MIMEType] decisionListener:listener];
    [l release];
}


- (void)didReceiveResponse:(NSURLResponse *)r
{
    ASSERT(shouldLoadAsEmptyDocument([r URL]) || ![self defersCallbacks]);
    ASSERT(shouldLoadAsEmptyDocument([r URL]) || ![frameLoader _defersCallbacks]);

    if (loadingMultipartContent) {
        [frameLoader _setupForReplaceByMIMEType:[r MIMEType]];
        [self clearResourceData];
    }
    
    if ([[r MIMEType] isEqualToString:@"multipart/x-mixed-replace"])
        loadingMultipartContent = YES;
        
    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    [frameLoader _setResponse:r];
    _contentLength = [r expectedContentLength];

    [self checkContentPolicyForResponse:r];
    [self release];
}

- (void)didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived allAtOnce:(BOOL)allAtOnce
{
    ASSERT(data);
    ASSERT([data length] != 0);
    ASSERT(![self defersCallbacks]);
    ASSERT(![frameLoader _defersCallbacks]);
 
    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    
    [super didReceiveData:data lengthReceived:lengthReceived allAtOnce:allAtOnce];
    _bytesReceived += [data length];

    [self release];
}

- (void)didFinishLoading
{
    ASSERT(shouldLoadAsEmptyDocument([frameLoader _URL]) || ![self defersCallbacks]);
    ASSERT(shouldLoadAsEmptyDocument([frameLoader _URL]) || ![frameLoader _defersCallbacks]);

    // Calls in this method will most likely result in a call to release, so we must retain.
    [self retain];

    [frameLoader _finishedLoading];
    [super didFinishLoading];

    [self release];
}

- (void)didFailWithError:(NSError *)error
{
    ASSERT(![self defersCallbacks]);
    ASSERT(![frameLoader _defersCallbacks]);

    [self receivedError:error];
}

- (NSURLRequest *)loadWithRequestNow:(NSURLRequest *)r
{
    BOOL shouldLoadEmptyBeforeRedirect = shouldLoadAsEmptyDocument([r URL]);

    ASSERT(connection == nil);
    ASSERT(shouldLoadEmptyBeforeRedirect || ![self defersCallbacks]);
    ASSERT(shouldLoadEmptyBeforeRedirect || ![frameLoader _defersCallbacks]);

    // Send this synthetic delegate callback since clients expect it, and
    // we no longer send the callback from within NSURLConnection for
    // initial requests.
    r = [self willSendRequest:r redirectResponse:nil];
    NSURL *URL = [r URL];
    BOOL shouldLoadEmpty = shouldLoadAsEmptyDocument(URL);

    if (shouldLoadEmptyBeforeRedirect && !shouldLoadEmpty && [self defersCallbacks]) {
        return r;
    }

    if (shouldLoadEmpty || [WebFrameLoader _representationExistsForURLScheme:[URL scheme]]) {
        NSString *MIMEType;
        if (shouldLoadEmpty) {
            MIMEType = @"text/html";
        } else {
            MIMEType = [WebFrameLoader _generatedMIMETypeForURLScheme:[URL scheme]];
        }

        NSURLResponse *resp = [[NSURLResponse alloc] initWithURL:URL MIMEType:MIMEType
            expectedContentLength:0 textEncodingName:nil];
        [self didReceiveResponse:resp];
        [resp release];
    } else {
        connection = [[NSURLConnection alloc] initWithRequest:r delegate:proxy];
    }

    return nil;
}

- (BOOL)loadWithRequest:(NSURLRequest *)r
{
    ASSERT(connection == nil);

    BOOL defer = [self defersCallbacks];
    if (defer) {
        NSURL *URL = [r URL];
        BOOL shouldLoadEmpty = shouldLoadAsEmptyDocument(URL);
        if (shouldLoadEmpty) {
            defer = NO;
        }
    }
    if (!defer) {
        r = [self loadWithRequestNow:r];
        if (r != nil) {
            // Started as an empty document, but was redirected to something non-empty.
            ASSERT([self defersCallbacks]);
            defer = YES;
        }
    }
    if (defer) {
        NSURLRequest *copy = [r copy];
        [_initialRequest release];
        _initialRequest = copy;
    }

    return YES;
}

- (void)setDefersCallbacks:(BOOL)defers
{
    [super setDefersCallbacks:defers];
    if (!defers) {
        NSURLRequest *r = _initialRequest;
        if (r != nil) {
            _initialRequest = nil;
            [self loadWithRequestNow:r];
            [r release];
        }
    }
}

@end
