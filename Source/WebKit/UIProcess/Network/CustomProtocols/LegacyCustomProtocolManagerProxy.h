/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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

#include "LegacyCustomProtocolID.h"
#include "MessageReceiver.h"
#include <wtf/CheckedRef.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(COCOA)
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
OBJC_CLASS WKCustomProtocolLoader;
#endif

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
} // namespace WebCore

namespace WebKit {

enum class CacheStoragePolicy : uint8_t;
class NetworkProcessProxy;

class LegacyCustomProtocolManagerProxy final : public IPC::MessageReceiver {
public:
    explicit LegacyCustomProtocolManagerProxy(NetworkProcessProxy&);
    ~LegacyCustomProtocolManagerProxy();

    void startLoading(LegacyCustomProtocolID, const WebCore::ResourceRequest&);
    void stopLoading(LegacyCustomProtocolID);

    void invalidate();

    void wasRedirectedToRequest(LegacyCustomProtocolID, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);
    void didReceiveResponse(LegacyCustomProtocolID, const WebCore::ResourceResponse&, CacheStoragePolicy);
    void didLoadData(LegacyCustomProtocolID, std::span<const uint8_t>);
    void didFailWithError(LegacyCustomProtocolID, const WebCore::ResourceError&);
    void didFinishLoading(LegacyCustomProtocolID);

    void ref() const;
    void deref() const;

private:
    Ref<NetworkProcessProxy> protectedProcess();

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    WeakRef<NetworkProcessProxy> m_networkProcessProxy;
};

} // namespace WebKit
