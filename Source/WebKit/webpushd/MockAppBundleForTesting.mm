/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "MockAppBundleForTesting.h"

#import "MockAppBundleRegistry.h"
#import <wtf/MainThread.h>

namespace WebPushD {

MockAppBundleForTesting::MockAppBundleForTesting(const String& originString, const String& hostAppBundleIdentifier, PushAppBundleClient& client)
    : PushAppBundle(client)
    , m_originString(originString)
    , m_hostAppBundleIdentifier(hostAppBundleIdentifier)
{
}

void MockAppBundleForTesting::checkForExistingBundle()
{
    callOnMainRunLoop([protectedThis = Ref { *this }, this] {
        bool exists = MockAppBundleRegistry::singleton().doesBundleExist(m_hostAppBundleIdentifier, m_originString);
        if (m_client)
            m_client->didCheckForExistingBundle(*this, exists ? PushAppBundleExists::Yes : PushAppBundleExists::No);
    });
}

void MockAppBundleForTesting::deleteExistingBundle()
{
    callOnMainRunLoop([protectedThis = Ref { *this }, this] {
        MockAppBundleRegistry::singleton().deleteBundle(m_hostAppBundleIdentifier, m_originString);
        if (m_client)
            m_client->didDeleteExistingBundleWithError(*this, nil);
    });
}

void MockAppBundleForTesting::createBundle()
{
    callOnMainRunLoop([protectedThis = Ref { *this }, this] {
        MockAppBundleRegistry::singleton().createBundle(m_hostAppBundleIdentifier, m_originString);
        if (m_client)
            m_client->didCreateAppBundle(*this, PushAppBundleCreationResult::Success);
    });
}

void MockAppBundleForTesting::stop()
{
}

} // namespace WebPushD
