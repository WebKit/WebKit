/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "WebLoader.h"

#import "WebCoreSystemInterface.h"
#import "WebDataProtocol.h"
#import "WebFrameLoader.h"
#import <Foundation/NSURLAuthenticationChallenge.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLResponse.h>
#import <wtf/Assertions.h>
#import <wtf/RefPtr.h>

using namespace WebCore;

@interface WebCoreResourceLoaderDelegate : NSObject <NSURLAuthenticationChallengeSender>
{
    WebResourceLoader* m_loader;
}
- (id)initWithLoader:(WebResourceLoader*)loader;
- (void)detachLoader;
@end

@interface NSURLConnection (NSURLConnectionTigerPrivate)
- (NSData *)_bufferedData;
@end

@interface NSURLProtocol (WebFoundationSecret) 
+ (void)_removePropertyForKey:(NSString *)key inRequest:(NSMutableURLRequest *)request;
@end

namespace WebCore {

static unsigned inNSURLConnectionCallback;
static bool NSURLConnectionSupportsBufferedData;

#ifndef NDEBUG
static bool isInitializingConnection;
#endif

WebResourceLoader::WebResourceLoader(WebFrameLoader *frameLoader)
    : m_reachedTerminalState(false)
    , m_signalledFinish(false)
    , m_cancelled(false)
    , m_frameLoader(frameLoader)
    , m_currentConnectionChallenge(nil)
    , m_defersCallbacks([frameLoader defersCallbacks])
{
    static bool initialized = false;
    if (!initialized) {
        NSURLConnectionSupportsBufferedData = [NSURLConnection instancesRespondToSelector:@selector(_bufferedData)];
        initialized = true;
    }
}

WebResourceLoader::~WebResourceLoader()
{
    ASSERT(m_reachedTerminalState);
    releaseDelegate();
}

void WebResourceLoader::releaseResources()
{
    ASSERT(!m_reachedTerminalState);
    
    // It's possible that when we release the handle, it will be
    // deallocated and release the last reference to this object.
    // We need to retain to avoid accessing the object after it
    // has been deallocated and also to avoid reentering this method.
    RefPtr<WebResourceLoader> protector(this);

    // We need to set reachedTerminalState to true before we release
    // the resources to prevent a double dealloc of WebView <rdar://problem/4372628>
    m_reachedTerminalState = true;

    m_identifier = nil;
    m_connection = nil;
    m_frameLoader = nil;
    m_resourceData = nil;

    releaseDelegate();
}

bool WebResourceLoader::load(NSURLRequest *r)
{
    ASSERT(m_connection == nil);
    ASSERT(![m_frameLoader.get() archiveLoadPendingForLoader:this]);
    
    m_originalURL = [r URL];
    
    NSURLRequest *clientRequest = willSendRequest(r, nil);
    if (clientRequest == nil) {
        didFail([m_frameLoader.get() cancelledErrorWithRequest:r]);
        return false;
    }
    r = clientRequest;
    
    if ([m_frameLoader.get() willUseArchiveForRequest:r originalURL:m_originalURL.get() loader:this])
        return true;
    
#ifndef NDEBUG
    isInitializingConnection = YES;
#endif
    NSURLConnection *connection = [[NSURLConnection alloc] initWithRequest:r delegate:delegate()];
#ifndef NDEBUG
    isInitializingConnection = NO;
#endif
    m_connection = connection;
    [connection release];
    if (defersCallbacks())
        wkSetNSURLConnectionDefersCallbacks(m_connection.get(), YES);

    return true;
}

void WebResourceLoader::setDefersCallbacks(bool defers)
{
    m_defersCallbacks = defers;
    wkSetNSURLConnectionDefersCallbacks(m_connection.get(), defers);
}

bool WebResourceLoader::defersCallbacks() const
{
    return m_defersCallbacks;
}

void WebResourceLoader::setFrameLoader(WebFrameLoader *fl)
{
    ASSERT(fl);
    m_frameLoader = fl;
    setDefersCallbacks([fl defersCallbacks]);
}

WebFrameLoader *WebResourceLoader::frameLoader() const
{
    return m_frameLoader.get();
}

void WebResourceLoader::addData(NSData *data, bool allAtOnce)
{
    if (allAtOnce) {
        NSMutableData *dataCopy = [data mutableCopy];
        m_resourceData = dataCopy;
        [dataCopy release];
        return;
    }
        
    if (NSURLConnectionSupportsBufferedData) {
        // Buffer data only if the connection has handed us the data because is has stopped buffering it.
        if (m_resourceData)
            [m_resourceData.get() appendData:data];
    } else {
        if (!m_resourceData) {
            NSMutableData *newData = [[NSMutableData alloc] init];
            m_resourceData = newData;
            [newData release];
        }
        [m_resourceData.get() appendData:data];
    }
}

NSData *WebResourceLoader::resourceData()
{
    if (m_resourceData)
        // Retain and autorelease resourceData since releaseResources (which releases resourceData) may be called 
        // before the caller of this method has an opportunity to retain the returned data (4070729).
        return [[m_resourceData.get() retain] autorelease];

    if (NSURLConnectionSupportsBufferedData)
        return [m_connection.get() _bufferedData];

    return nil;
}

void WebResourceLoader::clearResourceData()
{
    [m_resourceData.get() setLength:0];
}

NSURLRequest *WebResourceLoader::willSendRequest(NSURLRequest *newRequest, NSURLResponse *redirectResponse)
{
    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<WebResourceLoader> protector(this);

    ASSERT(!m_reachedTerminalState);
    NSMutableURLRequest *mutableRequest = [[newRequest mutableCopy] autorelease];
    
    newRequest = mutableRequest;

    // If we have a special "applewebdata" scheme URL we send a fake request to the delegate.
    bool haveDataSchemeRequest = false;
    NSMutableURLRequest *clientRequest = [mutableRequest _webDataRequestExternalRequest];
    if (!clientRequest)
        clientRequest = mutableRequest;
    else
        haveDataSchemeRequest = true;
    
    if (!m_identifier) {
        id identifier = [m_frameLoader.get() _identifierForInitialRequest:clientRequest];
        m_identifier = identifier;
        [identifier release];
    }

    NSURLRequest *updatedRequest = [m_frameLoader.get() _willSendRequest:clientRequest
        forResource:m_identifier.get() redirectResponse:redirectResponse];

    if (!haveDataSchemeRequest)
        newRequest = updatedRequest;
    else {
        // If the delegate modified the request use that instead of
        // our applewebdata request, otherwise use the original
        // applewebdata request.
        if (![updatedRequest isEqual:clientRequest]) {
            newRequest = updatedRequest;
        
            // The respondsToSelector: check is only necessary for people building/running prior to Tier 8A416.
            if ([NSURLProtocol respondsToSelector:@selector(_removePropertyForKey:inRequest:)] &&
                [newRequest isKindOfClass:[NSMutableURLRequest class]]) {
                NSMutableURLRequest *mr = (NSMutableURLRequest *)newRequest;
                [NSURLProtocol _removePropertyForKey:[NSURLRequest _webDataRequestPropertyKey] inRequest:mr];
            }

        }
    }

    // Old code used to autorelease rather than release, so do an autorelease here for now.
    // Eventually we should remove this.
    [[m_request.get() retain] autorelease];

    // Store a copy of the request.
    NSURLRequest *copy = [newRequest copy];
    m_request = copy;
    [copy release];

    return copy;
}

void WebResourceLoader::didReceiveAuthenticationChallenge(NSURLAuthenticationChallenge *challenge)
{
    ASSERT(!m_reachedTerminalState);
    ASSERT(!m_currentConnectionChallenge);
    ASSERT(!m_currentWebChallenge);

    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<WebResourceLoader> protector(this);

    m_currentConnectionChallenge = challenge;
    NSURLAuthenticationChallenge *webChallenge = [[NSURLAuthenticationChallenge alloc] initWithAuthenticationChallenge:challenge sender:delegate()];
    m_currentWebChallenge = webChallenge;

    [m_frameLoader.get() _didReceiveAuthenticationChallenge:webChallenge forResource:m_identifier.get()];

    [webChallenge release];
}

void WebResourceLoader::didCancelAuthenticationChallenge(NSURLAuthenticationChallenge *challenge)
{
    ASSERT(!m_reachedTerminalState);
    ASSERT(m_currentConnectionChallenge);
    ASSERT(m_currentWebChallenge);
    ASSERT(m_currentConnectionChallenge == challenge);

    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<WebResourceLoader> protector(this);

    [m_frameLoader.get() _didCancelAuthenticationChallenge:m_currentWebChallenge.get()
        forResource:m_identifier.get()];
}

void WebResourceLoader::didReceiveResponse(NSURLResponse *r)
{
    ASSERT(!m_reachedTerminalState);

    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<WebResourceLoader> protector(this);

    // If the URL is one of our whacky applewebdata URLs then
    // fake up a substitute URL to present to the delegate.
    if ([WebDataProtocol _webIsDataProtocolURL:[r URL]])
        r = [[[NSURLResponse alloc] initWithURL:[m_request.get() _webDataRequestExternalURL] MIMEType:[r MIMEType]
                expectedContentLength:[r expectedContentLength] textEncodingName:[r textEncodingName]] autorelease];

    m_response = r;

    [m_frameLoader.get() _didReceiveResponse:r forResource:m_identifier.get()];
}

void WebResourceLoader::didReceiveData(NSData *data, long long lengthReceived, bool allAtOnce)
{
    // The following assertions are not quite valid here, since a subclass
    // might override didReceiveData: in a way that invalidates them. This
    // happens with the steps listed in 3266216
    // ASSERT(con == connection);
    // ASSERT(!m_reachedTerminalState);

    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<WebResourceLoader> protector(this);
    
    addData(data, allAtOnce);
    
    [m_frameLoader.get() _didReceiveData:data contentLength:(int)lengthReceived forResource:m_identifier.get()];
}

void WebResourceLoader::willStopBufferingData(NSData *data)
{
    ASSERT(!m_resourceData);
    NSMutableData *copy = [data mutableCopy];
    m_resourceData = copy;
    [copy release];
}

void WebResourceLoader::signalFinish()
{
    m_signalledFinish = true;
    [m_frameLoader.get() _didFinishLoadingForResource:m_identifier.get()];
}

void WebResourceLoader::didFinishLoading()
{
    // If load has been cancelled after finishing (which could happen with a 
    // javascript that changes the window location), do nothing.
    if (m_cancelled)
        return;
    
    ASSERT(!m_reachedTerminalState);

    if (!m_signalledFinish)
        signalFinish();

    releaseResources();
}

void WebResourceLoader::didFail(NSError *error)
{
    if (m_cancelled)
        return;
    
    ASSERT(!m_reachedTerminalState);

    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<WebResourceLoader> protector(this);

    [m_frameLoader.get() _didFailLoadingWithError:error forResource:m_identifier.get()];

    releaseResources();
}

NSCachedURLResponse *WebResourceLoader::willCacheResponse(NSCachedURLResponse *cachedResponse)
{
    // When in private browsing mode, prevent caching to disk
    if ([cachedResponse storagePolicy] == NSURLCacheStorageAllowed && [m_frameLoader.get() _privateBrowsingEnabled]) {
        cachedResponse = [[[NSCachedURLResponse alloc] initWithResponse:[cachedResponse response]
                                                                   data:[cachedResponse data]
                                                               userInfo:[cachedResponse userInfo]
                                                          storagePolicy:NSURLCacheStorageAllowedInMemoryOnly] autorelease];
    }
    return cachedResponse;
}

void WebResourceLoader::cancel(NSError *error)
{
    ASSERT(!m_reachedTerminalState);

    // This flag prevents bad behavior when loads that finish cause the
    // load itself to be cancelled (which could happen with a javascript that 
    // changes the window location). This is used to prevent both the body
    // of this method and the body of connectionDidFinishLoading: running
    // for a single delegate. Cancelling wins.
    m_cancelled = true;
    
    m_currentConnectionChallenge = nil;
    m_currentWebChallenge = nil;

    [m_frameLoader.get() cancelPendingArchiveLoadForLoader:this];
    [m_connection.get() cancel];

    [m_frameLoader.get() _didFailLoadingWithError:error forResource:m_identifier.get()];

    releaseResources();
}

void WebResourceLoader::cancel()
{
    if (!m_reachedTerminalState)
        cancel(cancelledError());
}

void WebResourceLoader::setIdentifier(id identifier)
{
    m_identifier = identifier;
}

NSURLResponse *WebResourceLoader::response() const
{
    return m_response.get();
}

bool WebResourceLoader::inConnectionCallback()
{
    return inNSURLConnectionCallback != 0;
}

NSError *WebResourceLoader::cancelledError()
{
    return [m_frameLoader.get() cancelledErrorWithRequest:m_request.get()];
}

void WebResourceLoader::receivedCredential(NSURLAuthenticationChallenge *challenge, NSURLCredential *credential)
{
    ASSERT(challenge);
    if (challenge != m_currentWebChallenge)
        return;

    [[m_currentConnectionChallenge sender] useCredential:credential forAuthenticationChallenge:m_currentConnectionChallenge];

    m_currentConnectionChallenge = nil;
    m_currentWebChallenge = nil;
}

void WebResourceLoader::receivedRequestToContinueWithoutCredential(NSURLAuthenticationChallenge *challenge)
{
    ASSERT(challenge);
    if (challenge != m_currentWebChallenge)
        return;

    [[m_currentConnectionChallenge sender] continueWithoutCredentialForAuthenticationChallenge:m_currentConnectionChallenge];

    m_currentConnectionChallenge = nil;
    m_currentWebChallenge = nil;
}

void WebResourceLoader::receivedCancellation(NSURLAuthenticationChallenge *challenge)
{
    if (challenge != m_currentWebChallenge)
        return;

    cancel();
}

WebCoreResourceLoaderDelegate *WebResourceLoader::delegate()
{
    if (!m_delegate) {
        WebCoreResourceLoaderDelegate *d = [[WebCoreResourceLoaderDelegate alloc] initWithLoader:this];
        m_delegate = d;
        [d release];
    }
    return m_delegate.get();
}

void WebResourceLoader::releaseDelegate()
{
    if (!m_delegate)
        return;
    [m_delegate.get() detachLoader];
    m_delegate = nil;
}

}

@implementation WebCoreResourceLoaderDelegate

- (id)initWithLoader:(WebResourceLoader*)loader
{
    self = [self init];
    if (!self)
        return nil;
    m_loader = loader;
    return self;
}

- (void)detachLoader
{
    m_loader = nil;
}

- (NSURLRequest *)connection:(NSURLConnection *)con willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse
{
    if (!m_loader)
        return nil;
    ++inNSURLConnectionCallback;
    NSURLRequest *result = m_loader->willSendRequest(newRequest, redirectResponse);
    --inNSURLConnectionCallback;
    return result;
}

- (void)connection:(NSURLConnection *)con didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (!m_loader)
        return;
    ++inNSURLConnectionCallback;
    m_loader->didReceiveAuthenticationChallenge(challenge);
    --inNSURLConnectionCallback;
}

- (void)connection:(NSURLConnection *)con didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (!m_loader)
        return;
    ++inNSURLConnectionCallback;
    m_loader->didCancelAuthenticationChallenge(challenge);
    --inNSURLConnectionCallback;
}

- (void)connection:(NSURLConnection *)con didReceiveResponse:(NSURLResponse *)r
{
    if (!m_loader)
        return;
    ++inNSURLConnectionCallback;
    m_loader->didReceiveResponse(r);
    --inNSURLConnectionCallback;
}

- (void)connection:(NSURLConnection *)con didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    if (!m_loader)
        return;
    ++inNSURLConnectionCallback;
    m_loader->didReceiveData(data, lengthReceived, false);
    --inNSURLConnectionCallback;
}

- (void)connection:(NSURLConnection *)con willStopBufferingData:(NSData *)data
{
    if (!m_loader)
        return;
    ++inNSURLConnectionCallback;
    m_loader->willStopBufferingData(data);
    --inNSURLConnectionCallback;
}

- (void)connectionDidFinishLoading:(NSURLConnection *)con
{
    if (!m_loader)
        return;
    ++inNSURLConnectionCallback;
    m_loader->didFinishLoading();
    --inNSURLConnectionCallback;
}

- (void)connection:(NSURLConnection *)con didFailWithError:(NSError *)error
{
    if (!m_loader)
        return;
    ++inNSURLConnectionCallback;
    m_loader->didFail(error);
    --inNSURLConnectionCallback;
}

- (NSCachedURLResponse *)connection:(NSURLConnection *)connection willCacheResponse:(NSCachedURLResponse *)cachedResponse
{
#ifndef NDEBUG
    if (isInitializingConnection)
        LOG_ERROR("connection:willCacheResponse: was called inside of [NSURLConnection initWithRequest:delegate:] (40676250)");
#endif
    if (!m_loader)
        return nil;
    ++inNSURLConnectionCallback;
    NSCachedURLResponse *result = m_loader->willCacheResponse(cachedResponse);
    --inNSURLConnectionCallback;
    return result;
}

- (void)useCredential:(NSURLCredential *)credential forAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (!m_loader)
        return;
    m_loader->receivedCredential(challenge, credential);
}

- (void)continueWithoutCredentialForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (!m_loader)
        return;
    m_loader->receivedRequestToContinueWithoutCredential(challenge);
}

- (void)cancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (!m_loader)
        return;
    m_loader->receivedCancellation(challenge);
}

@end
