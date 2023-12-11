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
#import <wtf/CrossThreadCopier.h>

namespace WebKit {

static void platformInvalidate(RetainPtr<_SEGrant>&& platformGrant)
{
    if (![platformGrant isValid])
        return;

#if USE(EXTENSIONKIT)
    NSError *error = nil;
    if (![platformGrant invalidateWithError:&error])
        RELEASE_LOG_ERROR(ProcessCapabilities, "Invalidating grant %{public}@ failed with error: %{public}@", platformGrant.get(), error);
#endif
}

ExtensionCapabilityGrant::ExtensionCapabilityGrant(String environmentIdentifier)
    : m_environmentIdentifier { WTFMove(environmentIdentifier) }
{
}

ExtensionCapabilityGrant::ExtensionCapabilityGrant(String&& environmentIdentifier, RetainPtr<_SEGrant>&& platformGrant)
    : m_environmentIdentifier { WTFMove(environmentIdentifier) }
    , m_platformGrant { WTFMove(platformGrant) }
{
}

ExtensionCapabilityGrant::~ExtensionCapabilityGrant()
{
    setPlatformGrant(nil);
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
    return !m_platformGrant;
}

bool ExtensionCapabilityGrant::isValid() const
{
#if USE(EXTENSIONKIT)
    if ([m_platformGrant isValid])
        return true;
#endif
    return false;
}

void ExtensionCapabilityGrant::setPlatformGrant(RetainPtr<_SEGrant>&& platformGrant)
{
    platformInvalidate(std::exchange(m_platformGrant, WTFMove(platformGrant)));
}

} // namespace WebKit

#endif // ENABLE(EXTENSION_CAPABILITIES)
