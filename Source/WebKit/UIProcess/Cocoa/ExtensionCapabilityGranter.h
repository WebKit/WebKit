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

#include <wtf/CheckedPtr.h>
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class ExtensionCapability;
class ExtensionCapabilityGrant;
class GPUProcessProxy;
class MediaCapability;
class WebPageProxy;
class WebProcessProxy;

class ExtensionCapabilityGranter : public CanMakeWeakPtr<ExtensionCapabilityGranter> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ExtensionCapabilityGranter);
public:
    struct Client : public CanMakeCheckedPtr {
        virtual ~Client() = default;
        virtual RefPtr<GPUProcessProxy> gpuProcessForCapabilityGranter(const ExtensionCapabilityGranter&) = 0;
        virtual RefPtr<WebProcessProxy> webProcessForCapabilityGranter(const ExtensionCapabilityGranter&, const String& environmentIdentifier) = 0;
    };

    static UniqueRef<ExtensionCapabilityGranter> create(Client&);

    void grant(const ExtensionCapability&);
    void revoke(const ExtensionCapability&);

    void setMediaCapabilityActive(MediaCapability&, bool);
    void invalidateGrants(Vector<ExtensionCapabilityGrant>&&);

private:
    friend UniqueRef<ExtensionCapabilityGranter> WTF::makeUniqueRefWithoutFastMallocCheck(Client&);
    explicit ExtensionCapabilityGranter(Client&);

    CheckedPtr<Client> m_client;
};

} // namespace WebKit

#endif // ENABLE(EXTENSION_CAPABILITIES)
