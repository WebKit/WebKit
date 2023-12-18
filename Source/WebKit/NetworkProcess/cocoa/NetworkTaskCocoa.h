/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#import "NetworkDataTask.h"
#import <WebCore/FrameIdentifier.h>
#import <WebCore/PageIdentifier.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/ResourceResponse.h>
#import <WebCore/ShouldRelaxThirdPartyCookieBlocking.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSString;
OBJC_CLASS NSURLSessionTask;

namespace WebCore {
class RegistrableDomain;
}

namespace WebKit {

class NetworkTaskCocoa {
public:
    virtual ~NetworkTaskCocoa() = default;

    void willPerformHTTPRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, RedirectCompletionHandler&&);
    virtual std::optional<WebCore::FrameIdentifier> frameID() const = 0;
    virtual std::optional<WebCore::PageIdentifier> pageID() const = 0;

    WebCore::ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking() const { return m_shouldRelaxThirdPartyCookieBlocking; }

protected:
    NetworkTaskCocoa(NetworkSession&, WebCore::ShouldRelaxThirdPartyCookieBlocking);

    static NSHTTPCookieStorage *statelessCookieStorage();
    bool shouldApplyCookiePolicyForThirdPartyCloaking() const;
    void applyCookiePolicyForThirdPartyCloaking(const WebCore::ResourceRequest&);
    void blockCookies();
    void unblockCookies();
    static void updateTaskWithFirstPartyForSameSiteCookies(NSURLSessionTask*, const WebCore::ResourceRequest&);
    bool needsFirstPartyCookieBlockingLatchModeQuirk(const URL& firstPartyURL, const URL& requestURL, const URL& redirectingURL) const;
    static NSString *lastRemoteIPAddress(NSURLSessionTask *);
    static WebCore::RegistrableDomain lastCNAMEDomain(String);

    bool isAlwaysOnLoggingAllowed() const { return m_isAlwaysOnLoggingAllowed; }
    virtual NSURLSessionTask* task() const = 0;
    virtual WebCore::StoredCredentialsPolicy storedCredentialsPolicy() const = 0;

private:
    WeakPtr<NetworkSession> m_networkSession;
    bool m_hasBeenSetToUseStatelessCookieStorage { false };
    Seconds m_ageCapForCNAMECloakedCookies { 24_h * 7 };
    bool m_isAlwaysOnLoggingAllowed { false };
    WebCore::ShouldRelaxThirdPartyCookieBlocking m_shouldRelaxThirdPartyCookieBlocking { WebCore::ShouldRelaxThirdPartyCookieBlocking::No };
};

} // namespace WebKit
