/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebCookieManager.h"

#import "NetworkProcess.h"
#import "NetworkSession.h"
#import <WebCore/HTTPCookieAcceptPolicy.h>
#import <WebCore/NetworkStorageSession.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/ProcessPrivilege.h>

namespace WebKit {
using namespace WebCore;

static CFHTTPCookieStorageAcceptPolicy toCFHTTPCookieStorageAcceptPolicy(HTTPCookieAcceptPolicy policy)
{
    switch (policy) {
    case HTTPCookieAcceptPolicy::AlwaysAccept:
        return CFHTTPCookieStorageAcceptPolicyAlways;
    case HTTPCookieAcceptPolicy::Never:
        return CFHTTPCookieStorageAcceptPolicyNever;
    case HTTPCookieAcceptPolicy::OnlyFromMainDocumentDomain:
        return CFHTTPCookieStorageAcceptPolicyOnlyFromMainDocumentDomain;
    case HTTPCookieAcceptPolicy::ExclusivelyFromMainDocumentDomain:
        return CFHTTPCookieStorageAcceptPolicyExclusivelyFromMainDocumentDomain;
    }
    ASSERT_NOT_REACHED();

    return CFHTTPCookieStorageAcceptPolicyAlways;
}

void WebCookieManager::platformSetHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy policy, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    [[NSHTTPCookieStorage sharedHTTPCookieStorage] setCookieAcceptPolicy:static_cast<NSHTTPCookieAcceptPolicy>(policy)];
    saveCookies([NSHTTPCookieStorage sharedHTTPCookieStorage], [callbackAggregator] { });

    m_process.forEachNetworkStorageSession([&] (const auto& networkStorageSession) {
        if (auto cookieStorage = networkStorageSession.cookieStorage())
            CFHTTPCookieStorageSetCookieAcceptPolicy(cookieStorage.get(), toCFHTTPCookieStorageAcceptPolicy(policy));
        if (auto *storage = networkStorageSession.nsCookieStorage())
            saveCookies(storage, [callbackAggregator] { });
    });
}

} // namespace WebKit
