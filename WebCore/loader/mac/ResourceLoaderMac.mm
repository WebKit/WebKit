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
#import "SharedBuffer.h"
#import "WebCoreSystemInterface.h"
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

void ResourceLoader::setIdentifier(id identifier)
{
    m_identifier = identifier;
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

}

