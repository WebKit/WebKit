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
#import "ResourceLoader.h"

#import "FrameLoader.h"
#import "FrameMac.h"
#import "Page.h"
#import "ResourceError.h"
#import "ResourceHandle.h"
#import "ResourceRequest.h"
#import "ResourceResponse.h"
#import "WebCoreSystemInterface.h"
#import "WebDataProtocol.h"
#import <Foundation/NSURLAuthenticationChallenge.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLResponse.h>
#import <wtf/Assertions.h>
#import <wtf/RefPtr.h>

using namespace WebCore;

@interface NSURLProtocol (WebFoundationSecret) 
+ (void)_removePropertyForKey:(NSString *)key inRequest:(NSMutableURLRequest *)request;
@end

namespace WebCore {

ResourceLoader::ResourceLoader(Frame* frame)
    : m_reachedTerminalState(false)
    , m_cancelled(false)
    , m_calledDidFinishLoad(false)
    , m_frame(frame)
    , m_currentConnectionChallenge(nil)
    , m_defersLoading(frame->page()->defersLoading())
{
}

ResourceLoader::~ResourceLoader()
{
    ASSERT(m_reachedTerminalState);
}

void ResourceLoader::releaseResources()
{
    ASSERT(!m_reachedTerminalState);
    
    // It's possible that when we release the handle, it will be
    // deallocated and release the last reference to this object.
    // We need to retain to avoid accessing the object after it
    // has been deallocated and also to avoid reentering this method.
    RefPtr<ResourceLoader> protector(this);

    m_frame = 0;

    // We need to set reachedTerminalState to true before we release
    // the resources to prevent a double dealloc of WebView <rdar://problem/4372628>
    m_reachedTerminalState = true;

    m_identifier = nil;
    m_handle = 0;
    m_resourceData = nil;
}

bool ResourceLoader::load(NSURLRequest *r)
{
    ASSERT(!m_handle);
    ASSERT(!frameLoader()->isArchiveLoadPending(this));
    
    m_originalURL = [r URL];
    
    NSURLRequest *clientRequest = willSendRequest(r, nil);
    if (clientRequest == nil) {
        didFail(frameLoader()->cancelledError(r));
        return false;
    }
    r = clientRequest;
    
    if (frameLoader()->willUseArchive(this, r, m_originalURL.get()))
        return true;
    
    m_handle = ResourceHandle::create(r, this, m_frame.get(), m_defersLoading);

    return true;
}

void ResourceLoader::setDefersLoading(bool defers)
{
    m_defersLoading = defers;
    if (m_handle)
        m_handle->setDefersLoading(defers);
}

FrameLoader* ResourceLoader::frameLoader() const
{
    if (!m_frame)
        return 0;
    return m_frame->loader();
}

void ResourceLoader::addData(const char* data, int length, bool allAtOnce)
{
    if (allAtOnce) {
        m_resourceData.adopt([[NSMutableData alloc] initWithBytes:data length:length]);
        return;
    }
        
    if (ResourceHandle::supportsBufferedData()) {
        // Buffer data only if the connection has handed us the data because is has stopped buffering it.
        if (m_resourceData)
            [m_resourceData.get() appendBytes:data length:length];
    } else {
        if (!m_resourceData) 
            m_resourceData.adopt([[NSMutableData alloc] init]);

        [m_resourceData.get() appendBytes:data length:length];
    }
}

NSData *ResourceLoader::resourceData()
{
    if (m_resourceData)
        // Retain and autorelease resourceData since releaseResources (which releases resourceData) may be called 
        // before the caller of this method has an opportunity to retain the returned data (4070729).
        return [[m_resourceData.get() retain] autorelease];

    if (ResourceHandle::supportsBufferedData() && m_handle)
        return m_handle->bufferedData();
    
    return nil;
}

void ResourceLoader::clearResourceData()
{
    [m_resourceData.get() setLength:0];
}

NSURLRequest *ResourceLoader::willSendRequest(NSURLRequest *newRequest, NSURLResponse *redirectResponse)
{
    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<ResourceLoader> protector(this);

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
    
    if (!m_identifier)
        m_identifier = frameLoader()->identifierForInitialRequest(clientRequest);

    NSURLRequest *updatedRequest = frameLoader()->willSendRequest(this, clientRequest, redirectResponse);

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

void ResourceLoader::didReceiveAuthenticationChallenge(NSURLAuthenticationChallenge *challenge)
{
    ASSERT(!m_reachedTerminalState);
    ASSERT(!m_currentConnectionChallenge);
    ASSERT(!m_currentWebChallenge);

    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<ResourceLoader> protector(this);

    m_currentConnectionChallenge = challenge;
    NSURLAuthenticationChallenge *webChallenge = [[NSURLAuthenticationChallenge alloc] initWithAuthenticationChallenge:challenge sender:(id<NSURLAuthenticationChallengeSender>)m_handle->delegate()];
    m_currentWebChallenge = webChallenge;

    frameLoader()->didReceiveAuthenticationChallenge(this, webChallenge);

    [webChallenge release];
}

void ResourceLoader::didCancelAuthenticationChallenge(NSURLAuthenticationChallenge *challenge)
{
    ASSERT(!m_reachedTerminalState);
    ASSERT(m_currentConnectionChallenge);
    ASSERT(m_currentWebChallenge);
    ASSERT(m_currentConnectionChallenge == challenge);

    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<ResourceLoader> protector(this);

    frameLoader()->didCancelAuthenticationChallenge(this, m_currentWebChallenge.get());
}

void ResourceLoader::didReceiveResponse(NSURLResponse *r)
{
    ASSERT(!m_reachedTerminalState);

    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<ResourceLoader> protector(this);

    // If the URL is one of our whacky applewebdata URLs then
    // fake up a substitute URL to present to the delegate.
    if ([WebDataProtocol _webIsDataProtocolURL:[r URL]])
        r = [[[NSURLResponse alloc] initWithURL:[m_request.get() _webDataRequestExternalURL] MIMEType:[r MIMEType]
                expectedContentLength:[r expectedContentLength] textEncodingName:[r textEncodingName]] autorelease];

    m_response = r;

    frameLoader()->didReceiveResponse(this, r);
}

void ResourceLoader::didReceiveData(const char* data, int length, long long lengthReceived, bool allAtOnce)
{
    // The following assertions are not quite valid here, since a subclass
    // might override didReceiveData: in a way that invalidates them. This
    // happens with the steps listed in 3266216
    // ASSERT(con == connection);
    // ASSERT(!m_reachedTerminalState);

    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<ResourceLoader> protector(this);

    addData(data, length, allAtOnce);
    if (m_frame)
        frameLoader()->didReceiveData(this, data, length, lengthReceived);
}

void ResourceLoader::willStopBufferingData(const char* data, int length)
{
    ASSERT(!m_resourceData);
    m_resourceData.adopt([[NSMutableData alloc] initWithBytes:data length:length]);
}

void ResourceLoader::didFinishLoading()
{
    // If load has been cancelled after finishing (which could happen with a 
    // JavaScript that changes the window location), do nothing.
    if (m_cancelled)
        return;
    ASSERT(!m_reachedTerminalState);

    didFinishLoadingOnePart();
    releaseResources();
}

void ResourceLoader::didFinishLoadingOnePart()
{
    if (m_cancelled)
        return;
    ASSERT(!m_reachedTerminalState);

    if (m_calledDidFinishLoad)
        return;
    m_calledDidFinishLoad = true;
    frameLoader()->didFinishLoad(this);
}

void ResourceLoader::didFail(const ResourceError& error)
{
    if (m_cancelled)
        return;
    ASSERT(!m_reachedTerminalState);

    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<ResourceLoader> protector(this);

    frameLoader()->didFailToLoad(this, error);

    releaseResources();
}

NSCachedURLResponse *ResourceLoader::willCacheResponse(NSCachedURLResponse *cachedResponse)
{
    // When in private browsing mode, prevent caching to disk
    if ([cachedResponse storagePolicy] == NSURLCacheStorageAllowed && frameLoader()->privateBrowsingEnabled())
        cachedResponse = [[[NSCachedURLResponse alloc] initWithResponse:[cachedResponse response]
                                                                   data:[cachedResponse data]
                                                               userInfo:[cachedResponse userInfo]
                                                          storagePolicy:NSURLCacheStorageAllowedInMemoryOnly] autorelease];
    return cachedResponse;
}

void ResourceLoader::didCancel(const ResourceError& error)
{
    ASSERT(!m_cancelled);
    ASSERT(!m_reachedTerminalState);

    // This flag prevents bad behavior when loads that finish cause the
    // load itself to be cancelled (which could happen with a javascript that 
    // changes the window location). This is used to prevent both the body
    // of this method and the body of connectionDidFinishLoading: running
    // for a single delegate. Cancelling wins.
    m_cancelled = true;
    
    m_currentConnectionChallenge = nil;
    m_currentWebChallenge = nil;

    frameLoader()->cancelPendingArchiveLoad(this);
    if (m_handle)
        m_handle->cancel();

    frameLoader()->didFailToLoad(this, error);

    releaseResources();
}

void ResourceLoader::cancel()
{
    cancel(nil);
}

void ResourceLoader::cancel(NSError *error)
{
    if (m_reachedTerminalState)
        return;
    if (error)
        didCancel(error);
    else
        didCancel(cancelledError());
}

void ResourceLoader::setIdentifier(id identifier)
{
    m_identifier = identifier;
}

NSURLResponse *ResourceLoader::response() const
{
    return m_response.get();
}

NSError *ResourceLoader::cancelledError()
{
    return frameLoader()->cancelledError(m_request.get());
}

void ResourceLoader::receivedCredential(NSURLAuthenticationChallenge *challenge, NSURLCredential *credential)
{
    ASSERT(challenge);
    if (challenge != m_currentWebChallenge)
        return;

    [[m_currentConnectionChallenge sender] useCredential:credential forAuthenticationChallenge:m_currentConnectionChallenge];

    m_currentConnectionChallenge = nil;
    m_currentWebChallenge = nil;
}

void ResourceLoader::receivedRequestToContinueWithoutCredential(NSURLAuthenticationChallenge *challenge)
{
    ASSERT(challenge);
    if (challenge != m_currentWebChallenge)
        return;

    [[m_currentConnectionChallenge sender] continueWithoutCredentialForAuthenticationChallenge:m_currentConnectionChallenge];

    m_currentConnectionChallenge = nil;
    m_currentWebChallenge = nil;
}

void ResourceLoader::receivedCancellation(NSURLAuthenticationChallenge *challenge)
{
    if (challenge != m_currentWebChallenge)
        return;

    cancel();
}

void ResourceLoader::willSendRequest(ResourceHandle*, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    request = willSendRequest(request.nsURLRequest(), redirectResponse.nsURLResponse());
}

void ResourceLoader::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
    didReceiveResponse(response.nsURLResponse());
}

void ResourceLoader::didReceiveData(ResourceHandle*, const char* data, int length, int lengthReceived)
{
    didReceiveData(data, length, lengthReceived, false);
}

void ResourceLoader::didFinishLoading(ResourceHandle*)
{
    didFinishLoading();
}

void ResourceLoader::didFail(ResourceHandle*, const ResourceError& error)
{
    didFail(error);
}

}

