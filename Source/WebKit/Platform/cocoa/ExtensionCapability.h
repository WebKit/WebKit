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

#if ENABLE(EXTENSION_CAPABILITIES)

#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS BEProcessCapability;
#if USE(LEGACY_EXTENSIONKIT_SPI)
OBJC_CLASS _SECapability;
#endif

namespace WebKit {

#if USE(LEGACY_EXTENSIONKIT_SPI)
using PlatformCapability = std::variant<RetainPtr<BEProcessCapability>, RetainPtr<_SECapability>>;
#else
using PlatformCapability = RetainPtr<BEProcessCapability>;
#endif

class ExtensionCapability {
public:
    virtual ~ExtensionCapability() = default;
    virtual String environmentIdentifier() const = 0;
    const PlatformCapability& platformCapability() const { return m_platformCapability; }

    bool hasPlatformCapability() const { return platformCapabilityIsValid(m_platformCapability); }

    static bool platformCapabilityIsValid(const PlatformCapability& platformCapability)
    {
#if USE(LEGACY_EXTENSIONKIT_SPI)
        return WTF::switchOn(platformCapability, [] (auto& capability) {
            return !!capability;
        });
#else
        return !!platformCapability;
#endif
    }

protected:
    ExtensionCapability() = default;
    void setPlatformCapability(PlatformCapability&& capability) { m_platformCapability = WTFMove(capability); }

private:
    PlatformCapability m_platformCapability;
};

} // namespace WebKit

#endif // ENABLE(EXTENSION_CAPABILITIES)
