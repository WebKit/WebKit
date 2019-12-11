/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
#import "config.h"
#import "AuthenticationMac.h"

#import "AuthenticationChallenge.h"
#import "AuthenticationClient.h"
#import "Credential.h"
#import <Foundation/NSURLAuthenticationChallenge.h>
#import <Foundation/NSURLProtectionSpace.h>

using namespace WebCore;

@interface WebCoreAuthenticationClientAsChallengeSender : NSObject <NSURLAuthenticationChallengeSender>
{
    AuthenticationClient* m_client;
}
- (id)initWithAuthenticationClient:(AuthenticationClient*)client;
- (AuthenticationClient*)client;
- (void)detachClient;
@end

@implementation WebCoreAuthenticationClientAsChallengeSender

- (id)initWithAuthenticationClient:(AuthenticationClient*)client
{
    self = [self init];
    if (!self)
        return nil;
    m_client = client;
    return self;
}

- (AuthenticationClient*)client
{
    return m_client;
}

- (void)detachClient
{
    m_client = 0;
}

- (void)performDefaultHandlingForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (m_client)
        m_client->receivedRequestToPerformDefaultHandling(core(challenge));
}

- (void)rejectProtectionSpaceAndContinueWithChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (m_client)
        m_client->receivedChallengeRejection(core(challenge));
}

- (void)useCredential:(NSURLCredential *)credential forAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (m_client)
        m_client->receivedCredential(core(challenge), Credential(credential));
}

- (void)continueWithoutCredentialForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (m_client)
        m_client->receivedRequestToContinueWithoutCredential(core(challenge));
}

- (void)cancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (m_client)
        m_client->receivedCancellation(core(challenge));
}

@end

namespace WebCore {

AuthenticationChallenge::AuthenticationChallenge(const ProtectionSpace& protectionSpace,
                                                 const Credential& proposedCredential,
                                                 unsigned previousFailureCount,
                                                 const ResourceResponse& response,
                                                 const ResourceError& error)
    : AuthenticationChallengeBase(protectionSpace,
                                  proposedCredential,
                                  previousFailureCount,
                                  response,
                                  error)
{
}

AuthenticationChallenge::AuthenticationChallenge(NSURLAuthenticationChallenge *challenge)
    : AuthenticationChallengeBase(ProtectionSpace([challenge protectionSpace]),
                                  Credential([challenge proposedCredential]),
                                  [challenge previousFailureCount],
                                  [challenge failureResponse],
                                  [challenge error])
    , m_sender([challenge sender])
    , m_nsChallenge(challenge)
{
}

void AuthenticationChallenge::setAuthenticationClient(AuthenticationClient* client)
{
    if (client) {
        m_sender = adoptNS([[WebCoreAuthenticationClientAsChallengeSender alloc] initWithAuthenticationClient:client]);
        if (m_nsChallenge)
            m_nsChallenge = adoptNS([[NSURLAuthenticationChallenge alloc] initWithAuthenticationChallenge:m_nsChallenge.get() sender:m_sender.get()]);
    } else {
        if ([m_sender.get() isMemberOfClass:[WebCoreAuthenticationClientAsChallengeSender class]])
            [(WebCoreAuthenticationClientAsChallengeSender *)m_sender.get() detachClient];
    }
}

AuthenticationClient* AuthenticationChallenge::authenticationClient() const
{
    if ([m_sender.get() isMemberOfClass:[WebCoreAuthenticationClientAsChallengeSender class]])
        return [static_cast<WebCoreAuthenticationClientAsChallengeSender*>(m_sender.get()) client];
    
    return nullptr;
}

bool AuthenticationChallenge::platformCompare(const AuthenticationChallenge& a, const AuthenticationChallenge& b)
{
    if (a.sender() != b.sender())
        return false;
        
    if (a.nsURLAuthenticationChallenge() != b.nsURLAuthenticationChallenge())
        return false;

    return true;
}

NSURLAuthenticationChallenge *mac(const AuthenticationChallenge& coreChallenge)
{
    if (coreChallenge.nsURLAuthenticationChallenge())
        return coreChallenge.nsURLAuthenticationChallenge();
        
    return [[[NSURLAuthenticationChallenge alloc] initWithProtectionSpace:coreChallenge.protectionSpace().nsSpace() proposedCredential:coreChallenge.proposedCredential().nsCredential() previousFailureCount:coreChallenge.previousFailureCount() failureResponse:coreChallenge.failureResponse().nsURLResponse() error:coreChallenge.error() sender:coreChallenge.sender()] autorelease];
}

AuthenticationChallenge core(NSURLAuthenticationChallenge *macChallenge)
{
    return AuthenticationChallenge(macChallenge);
}

} // namespace WebCore
