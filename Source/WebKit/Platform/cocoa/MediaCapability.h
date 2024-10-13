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
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS BEMediaEnvironment;

namespace WebKit {
class MediaCapability;
}

namespace WebKit {

class ExtensionCapabilityGrant;

class MediaCapability final : public ExtensionCapability, public RefCountedAndCanMakeWeakPtr<MediaCapability> {
    WTF_MAKE_NONCOPYABLE(MediaCapability);
public:
    static Ref<MediaCapability> create(URL&&);

    enum class State : uint8_t {
        Inactive,
        Activating,
        Active,
        Deactivating,
    };

    State state() const { return m_state; }
    void setState(State state) { m_state = state; }
    bool isActivatingOrActive() const;

    const URL& webPageURL() const { return m_webPageURL; }

    // ExtensionCapability
    String environmentIdentifier() const final;

    BEMediaEnvironment *platformMediaEnvironment() const { return m_mediaEnvironment.get(); }

private:
    explicit MediaCapability(URL&&);

    State m_state { State::Inactive };
    URL m_webPageURL;
    RetainPtr<BEMediaEnvironment> m_mediaEnvironment;
};

} // namespace WebKit

#endif // ENABLE(EXTENSION_CAPABILITIES)
