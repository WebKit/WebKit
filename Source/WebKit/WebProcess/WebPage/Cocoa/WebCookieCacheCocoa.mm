/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "WebCookieCache.h"

#import "NetworkProcessConnection.h"
#import "WebProcess.h"
#import <WebCore/NetworkStorageSession.h>
#import <wtf/text/MakeString.h>

namespace WebKit {

using namespace WebCore;

NetworkStorageSession& WebCookieCache::inMemoryStorageSession()
{
    if (!m_inMemoryStorageSession) {
        String sessionName = makeString("WebKitInProcessStorage-"_s, getCurrentProcessID());
        auto cookieAcceptPolicy = WebProcess::singleton().ensureNetworkProcessConnection().cookieAcceptPolicy();
        auto storageSession = WebCore::createPrivateStorageSession(sessionName.createCFString().get(), cookieAcceptPolicy);
        auto cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, storageSession.get()));
        m_inMemoryStorageSession = makeUnique<NetworkStorageSession>(WebProcess::singleton().sessionID(), WTFMove(storageSession), WTFMove(cookieStorage), NetworkStorageSession::IsInMemoryCookieStore::Yes);
#if ENABLE(OPT_IN_PARTITIONED_COOKIES) && defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) && CFN_COOKIE_ACCEPTS_POLICY_PARTITION
        m_inMemoryStorageSession->setOptInCookiePartitioningEnabled(m_optInCookiePartitioningEnabled);
#endif
    }
    return *m_inMemoryStorageSession;
}

void WebCookieCache::setOptInCookiePartitioningEnabled(bool enabled)
{
#if ENABLE(OPT_IN_PARTITIONED_COOKIES) && defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) && CFN_COOKIE_ACCEPTS_POLICY_PARTITION
    m_optInCookiePartitioningEnabled = enabled;
    if (m_inMemoryStorageSession)
        m_inMemoryStorageSession->setOptInCookiePartitioningEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

} // namespace WebKit
