/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "NetworkSessionCreationParameters.h"

#include "ArgumentCoders.h"

#if PLATFORM(COCOA)
#include "ArgumentCodersCF.h"
#endif

#if USE(CURL)
#include "WebCoreArgumentCoders.h"
#endif

namespace WebKit {

NetworkSessionCreationParameters NetworkSessionCreationParameters::privateSessionParameters(const PAL::SessionID& sessionID)
{
    return { sessionID, { }, AllowsCellularAccess::Yes
#if PLATFORM(COCOA)
        , { }, { }, { }, false, { }, { }, { }
#endif
#if USE(CURL)
        , { }
#endif
    };
}

void NetworkSessionCreationParameters::encode(IPC::Encoder& encoder) const
{
    encoder << sessionID;
    encoder << boundInterfaceIdentifier;
    encoder << allowsCellularAccess;
#if PLATFORM(COCOA)
    IPC::encode(encoder, proxyConfiguration.get());
    encoder << sourceApplicationBundleIdentifier;
    encoder << sourceApplicationSecondaryIdentifier;
    encoder << shouldLogCookieInformation;
    encoder << loadThrottleLatency;
    encoder << httpProxy;
    encoder << httpsProxy;
#endif
#if USE(CURL)
    encoder << proxySettings;
#endif
}

Optional<NetworkSessionCreationParameters> NetworkSessionCreationParameters::decode(IPC::Decoder& decoder)
{
    PAL::SessionID sessionID;
    if (!decoder.decode(sessionID))
        return WTF::nullopt;
    
    Optional<String> boundInterfaceIdentifier;
    decoder >> boundInterfaceIdentifier;
    if (!boundInterfaceIdentifier)
        return WTF::nullopt;
    
    Optional<AllowsCellularAccess> allowsCellularAccess;
    decoder >> allowsCellularAccess;
    if (!allowsCellularAccess)
        return WTF::nullopt;
    
#if PLATFORM(COCOA)
    RetainPtr<CFDictionaryRef> proxyConfiguration;
    if (!IPC::decode(decoder, proxyConfiguration))
        return WTF::nullopt;
    
    Optional<String> sourceApplicationBundleIdentifier;
    decoder >> sourceApplicationBundleIdentifier;
    if (!sourceApplicationBundleIdentifier)
        return WTF::nullopt;
    
    Optional<String> sourceApplicationSecondaryIdentifier;
    decoder >> sourceApplicationSecondaryIdentifier;
    if (!sourceApplicationSecondaryIdentifier)
        return WTF::nullopt;
    
    Optional<bool> shouldLogCookieInformation;
    decoder >> shouldLogCookieInformation;
    if (!shouldLogCookieInformation)
        return WTF::nullopt;
    
    Optional<Seconds> loadThrottleLatency;
    decoder >> loadThrottleLatency;
    if (!loadThrottleLatency)
        return WTF::nullopt;
    
    Optional<URL> httpProxy;
    decoder >> httpProxy;
    if (!httpProxy)
        return WTF::nullopt;

    Optional<URL> httpsProxy;
    decoder >> httpsProxy;
    if (!httpsProxy)
        return WTF::nullopt;
#endif
    
#if USE(CURL)
    Optional<WebCore::CurlProxySettings> proxySettings;
    decoder >> proxySettings;
    if (!proxySettings)
        return WTF::nullopt;
#endif
    
    return {{
        sessionID
        , WTFMove(*boundInterfaceIdentifier)
        , WTFMove(*allowsCellularAccess)
#if PLATFORM(COCOA)
        , WTFMove(proxyConfiguration)
        , WTFMove(*sourceApplicationBundleIdentifier)
        , WTFMove(*sourceApplicationSecondaryIdentifier)
        , WTFMove(*shouldLogCookieInformation)
        , WTFMove(*loadThrottleLatency)
        , WTFMove(*httpProxy)
        , WTFMove(*httpsProxy)
#endif
#if USE(CURL)
        , WTFMove(*proxySettings)
#endif
    }};
}

} // namespace WebKit
