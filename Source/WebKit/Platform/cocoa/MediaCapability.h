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

#include "ExtensionCapability.h"
#include <WebCore/RegistrableDomain.h>
#include <wtf/Forward.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class ExtensionCapabilityGrant;

using WebCore::RegistrableDomain;

class MediaCapability final : public ExtensionCapability, public CanMakeWeakPtr<MediaCapability> {
public:
    explicit MediaCapability(RegistrableDomain);
    explicit MediaCapability(const URL&);

    enum class State : uint8_t {
        Inactive,
        Activating,
        Active,
        Deactivating,
    };

    State state() const { return m_state; }
    void setState(State state) { m_state = state; }

    const RegistrableDomain& registrableDomain() const { return m_registrableDomain; }

    // ExtensionCapability
    String environmentIdentifier() const final;
    RetainPtr<_SECapabilities> platformCapability() const final { return m_platformCapability.get(); }

private:
    State m_state { State::Inactive };
    RegistrableDomain m_registrableDomain;
    RetainPtr<_SECapabilities> m_platformCapability;
};

} // namespace WebKit

#endif // ENABLE(EXTENSION_CAPABILITIES)
