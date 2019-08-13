/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "NetworkRTCProvider.h"

#if USE(LIBWEBRTC)

#include "NetworkSessionCocoa.h"
#include <WebCore/LibWebRTCMacros.h>
#include <webrtc/rtc_base/asyncpacketsocket.h>
#include <webrtc/rtc_base/logging.h>

namespace WebKit {

static inline bool isEnabled(CFDictionaryRef proxy, CFStringRef key)
{
    auto enabled = static_cast<CFNumberRef>(CFDictionaryGetValue(proxy, key));

    if (!enabled)
        return false;

    int result;
    return CFNumberGetValue(enabled, kCFNumberIntType, &result);
    return !!result;
}

static inline rtc::ProxyInfo createRTCProxy(CFDictionaryRef proxy, rtc::ProxyType type, CFStringRef nameKey, CFStringRef portKey)
{
    rtc::ProxyInfo info;
    info.type = type;

    auto proxyHostName = static_cast<CFStringRef>(CFDictionaryGetValue(proxy, nameKey));
    if (!proxyHostName)
        return { };
    auto portNumber = static_cast<CFNumberRef>(CFDictionaryGetValue(proxy, portKey));
    if (!portNumber)
        return { };

    int proxyPort;
    if (!CFNumberGetValue(portNumber, kCFNumberSInt32Type, &proxyPort))
        return { };

    info.address = { std::string([(__bridge NSString *)proxyHostName UTF8String]), proxyPort };
    info.autodetect = false;
    return info;
}

rtc::ProxyInfo NetworkRTCProvider::proxyInfoFromSession(const RTCNetwork::SocketAddress&, NetworkSession& session)
{
    // FIXME: We should check for kCFNetworkProxiesExceptionsList to decide whether to use proxy or not.
    // FIXME: We should also get username/password for authentication cases.
    auto proxyDictionary = static_cast<NetworkSessionCocoa&>(session).proxyConfiguration();
    if (!proxyDictionary)
        proxyDictionary = CFNetworkCopySystemProxySettings();
#if PLATFORM(MAC)
    if (isEnabled(proxyDictionary, kCFNetworkProxiesHTTPSEnable))
        return createRTCProxy(proxyDictionary, rtc::PROXY_HTTPS, kCFNetworkProxiesHTTPSProxy, kCFNetworkProxiesHTTPSPort);

    if (isEnabled(proxyDictionary, kCFNetworkProxiesSOCKSEnable))
        return createRTCProxy(proxyDictionary, rtc::PROXY_SOCKS5, kCFNetworkProxiesSOCKSProxy, kCFNetworkProxiesSOCKSPort);
#endif
#if PLATFORM(IOS)
    if (isEnabled(proxyDictionary, kCFNetworkProxiesHTTPEnable))
        return createRTCProxy(proxyDictionary, rtc::PROXY_HTTPS, kCFNetworkProxiesHTTPProxy, kCFNetworkProxiesHTTPPort);
#endif
    return { };
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
