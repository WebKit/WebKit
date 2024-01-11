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

#import "config.h"
#import "MediaCapability.h"

#if ENABLE(EXTENSION_CAPABILITIES)

#import <WebCore/RegistrableDomain.h>
#import <wtf/text/WTFString.h>

#import "ExtensionKitSoftLink.h"

namespace WebKit {

using WebCore::RegistrableDomain;

static RetainPtr<_SECapability> createPlatformCapability(const URL& url)
{
#if USE(EXTENSIONKIT)
    if (_SECapability *capability = [get_SECapabilityClass() mediaWithWebsite:RegistrableDomain(url).string()])
        return capability;
#endif

    UNUSED_PARAM(url);
    return nil;
}

MediaCapability::MediaCapability(URL url)
    : m_url { WTFMove(url) }
    , m_platformCapability { createPlatformCapability(m_url) }
{
}

bool MediaCapability::isActivatingOrActive() const
{
    switch (m_state) {
    case State::Inactive:
    case State::Deactivating:
        return false;
    case State::Activating:
    case State::Active:
        return true;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

RegistrableDomain MediaCapability::registrableDomain() const
{
    return RegistrableDomain { m_url };
}

String MediaCapability::environmentIdentifier() const
{
#if USE(EXTENSIONKIT)
    if (NSString *mediaEnvironment = [m_platformCapability mediaEnvironment])
        return mediaEnvironment;
#endif

    return { };
}

} // namespace WebKit

#endif // ENABLE(EXTENSION_CAPABILITIES)
