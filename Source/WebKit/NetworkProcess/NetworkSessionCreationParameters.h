/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "ArgumentCoders.h"
#include <pal/SessionID.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include "ArgumentCodersCF.h"
#endif

namespace WebKit {

class LegacyCustomProtocolManager;

enum class AllowsCellularAccess { No, Yes };
    
struct NetworkSessionCreationParameters {
    void encode(IPC::Encoder&) const;
    static std::optional<NetworkSessionCreationParameters> decode(IPC::Decoder&);
    
    PAL::SessionID sessionID { PAL::SessionID::defaultSessionID() };
    String boundInterfaceIdentifier;
    AllowsCellularAccess allowsCellularAccess { AllowsCellularAccess::Yes };
#if PLATFORM(COCOA)
    RetainPtr<CFDictionaryRef> proxyConfiguration;
    String sourceApplicationBundleIdentifier;
    String sourceApplicationSecondaryIdentifier;
#endif
};

inline void NetworkSessionCreationParameters::encode(IPC::Encoder& encoder) const
{
    encoder << sessionID;
    encoder << boundInterfaceIdentifier;
    encoder << allowsCellularAccess;
#if PLATFORM(COCOA)
    IPC::encode(encoder, proxyConfiguration.get());
    encoder << sourceApplicationBundleIdentifier;
    encoder << sourceApplicationSecondaryIdentifier;
#endif
}

inline std::optional<NetworkSessionCreationParameters> NetworkSessionCreationParameters::decode(IPC::Decoder& decoder)
{
    PAL::SessionID sessionID;
    if (!decoder.decode(sessionID))
        return std::nullopt;

    std::optional<String> boundInterfaceIdentifier;
    decoder >> boundInterfaceIdentifier;
    if (!boundInterfaceIdentifier)
        return std::nullopt;

    std::optional<AllowsCellularAccess> allowsCellularAccess;
    decoder >> allowsCellularAccess;
    if (!allowsCellularAccess)
        return std::nullopt;

#if PLATFORM(COCOA)
    RetainPtr<CFDictionaryRef> proxyConfiguration;
    if (!IPC::decode(decoder, proxyConfiguration))
        return std::nullopt;
    
    std::optional<String> sourceApplicationBundleIdentifier;
    decoder >> sourceApplicationBundleIdentifier;
    if (!sourceApplicationBundleIdentifier)
        return std::nullopt;
    
    std::optional<String> sourceApplicationSecondaryIdentifier;
    decoder >> sourceApplicationSecondaryIdentifier;
    if (!sourceApplicationSecondaryIdentifier)
        return std::nullopt;
#endif
    
    return {{
        sessionID
        , WTFMove(*boundInterfaceIdentifier)
        , WTFMove(*allowsCellularAccess)
#if PLATFORM(COCOA)
        , WTFMove(proxyConfiguration)
        , WTFMove(*sourceApplicationBundleIdentifier)
        , WTFMove(*sourceApplicationSecondaryIdentifier)
#endif
    }};
}

} // namespace WebKit

namespace WTF {
template<> struct EnumTraits<WebKit::AllowsCellularAccess> {
    using values = EnumValues<
        WebKit::AllowsCellularAccess,
        WebKit::AllowsCellularAccess::No,
        WebKit::AllowsCellularAccess::Yes
    >;
};
}
