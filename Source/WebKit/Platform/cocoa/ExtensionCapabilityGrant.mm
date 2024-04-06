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
#import "ExtensionCapabilityGrant.h"

#if ENABLE(EXTENSION_CAPABILITIES)

#import "ExtensionKitSPI.h"
#import "Logging.h"
#import <BrowserEngineKit/BrowserEngineKit.h>
#import <wtf/CrossThreadCopier.h>

namespace WebKit {

static void platformInvalidate(const PlatformGrant& platformGrant)
{
#if USE(LEGACY_EXTENSIONKIT_SPI)
    bool isValid = WTF::switchOn(platformGrant, [&] (auto& grant) {
        return [grant isValid];
    });
    if (!isValid)
        return;

    WTF::switchOn(platformGrant, [&] (const RetainPtr<BEProcessCapabilityGrant>& grant) {
        if (![grant invalidate])
            RELEASE_LOG_ERROR(ProcessCapabilities, "Invalidating grant %{public}@ failed", grant.get());
    }, [&] (const RetainPtr<_SEGrant>& grant) {
        if (![grant invalidateWithError:nil])
            RELEASE_LOG_ERROR(ProcessCapabilities, "Invalidating grant %{public}@ failed", grant.get());
    });
#else
    if (![platformGrant isValid])
        return;
    if (![platformGrant invalidate])
        RELEASE_LOG_ERROR(ProcessCapabilities, "Invalidating grant %{public}@ failed", platformGrant.get());
#endif
}

ExtensionCapabilityGrant::ExtensionCapabilityGrant(String environmentIdentifier)
    : m_environmentIdentifier { WTFMove(environmentIdentifier) }
{
}

ExtensionCapabilityGrant::ExtensionCapabilityGrant(String&& environmentIdentifier, PlatformGrant&& platformGrant)
    : m_environmentIdentifier { WTFMove(environmentIdentifier) }
    , m_platformGrant { WTFMove(platformGrant) }
{
}

ExtensionCapabilityGrant::~ExtensionCapabilityGrant()
{
    setPlatformGrant({ });
}

ExtensionCapabilityGrant& ExtensionCapabilityGrant::operator=(ExtensionCapabilityGrant&& grant)
{
    platformInvalidate(m_platformGrant);
    m_environmentIdentifier = WTFMove(grant.m_environmentIdentifier);
    m_platformGrant = WTFMove(grant.m_platformGrant);
    return *this;
}

ExtensionCapabilityGrant ExtensionCapabilityGrant::isolatedCopy() &&
{
    return {
        crossThreadCopy(WTFMove(m_environmentIdentifier)),
        WTFMove(m_platformGrant)
    };
}

bool ExtensionCapabilityGrant::isEmpty() const
{
#if USE(LEGACY_EXTENSIONKIT_SPI)
    return WTF::switchOn(m_platformGrant, [] (auto& grant) {
        return !grant;
    });
#else
    return !m_platformGrant;
#endif
}

bool ExtensionCapabilityGrant::isValid() const
{
#if USE(LEGACY_EXTENSIONKIT_SPI)
    return WTF::switchOn(m_platformGrant, [] (auto& grant) {
        return [grant isValid];
    });
#else
    return [m_platformGrant isValid];
#endif
}

void ExtensionCapabilityGrant::setPlatformGrant(PlatformGrant&& platformGrant)
{
    platformInvalidate(std::exchange(m_platformGrant, WTFMove(platformGrant)));
}

void ExtensionCapabilityGrant::invalidate()
{
    setPlatformGrant({ });
}

} // namespace WebKit

#endif // ENABLE(EXTENSION_CAPABILITIES)
