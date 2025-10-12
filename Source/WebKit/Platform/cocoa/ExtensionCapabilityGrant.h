/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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

#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

OBJC_PROTOCOL(BEProcessCapabilityGrant);
OBJC_PROTOCOL(_SEGrant);

namespace WebKit {

using PlatformGrant = RetainPtr<BEProcessCapabilityGrant>;

class ExtensionCapabilityGrant {
public:
    ExtensionCapabilityGrant() = default;
    ExtensionCapabilityGrant(ExtensionCapabilityGrant&&) = default;
    explicit ExtensionCapabilityGrant(String environmentIdentifier);
    ~ExtensionCapabilityGrant();

    ExtensionCapabilityGrant& operator=(ExtensionCapabilityGrant&&) = default;
    ExtensionCapabilityGrant isolatedCopy() &&;

    const String& environmentIdentifier() const { return m_environmentIdentifier; }
    bool isEmpty() const;
    bool isValid() const;

    void setPlatformGrant(PlatformGrant&&);

    void invalidate();

private:
    ExtensionCapabilityGrant(String&&, PlatformGrant&&);

    String m_environmentIdentifier;
    PlatformGrant m_platformGrant;
};

} // namespace WebKit

#endif // ENABLE(EXTENSION_CAPABILITIES)
