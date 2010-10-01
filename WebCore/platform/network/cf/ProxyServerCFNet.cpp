/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "ProxyServer.h"

#include "KURL.h"
#include <wtf/RetainPtr.h>

namespace WebCore {

#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
static void addProxyServersForURL(Vector<ProxyServer>& proxyServers, const KURL& url)
{
    RetainPtr<CFDictionaryRef> proxySettings(AdoptCF, CFNetworkCopySystemProxySettings());
    if (!proxySettings)
        return;

    RetainPtr<CFURLRef> cfURL(AdoptCF, url.createCFURL());
    RetainPtr<CFArrayRef> proxiesForURL(AdoptCF, CFNetworkCopyProxiesForURL(cfURL.get(), proxySettings.get()));
    if (!proxiesForURL)
        return;

    CFIndex numProxies = CFArrayGetCount(proxiesForURL.get());
    for (CFIndex i = 0; i < numProxies; ++i) {
        CFDictionaryRef proxyDictionary = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(proxiesForURL.get(), i));

        ProxyServer::Type type = ProxyServer::Direct;
        CFStringRef typeString = static_cast<CFStringRef>(CFDictionaryGetValue(proxyDictionary, kCFProxyTypeKey));
        if (CFEqual(typeString, kCFProxyTypeAutoConfigurationURL)) {
            // FIXME: Handle PAC URLs.
            continue;
        } 
        
        if (CFEqual(typeString, kCFProxyTypeNone)) {
            proxyServers.append(ProxyServer(ProxyServer::Direct, String(), -1));
            continue;
        }
        
        if (CFEqual(typeString, kCFProxyTypeHTTP))
            type = ProxyServer::HTTP;
        else if (CFEqual(typeString, kCFProxyTypeHTTPS))
            type = ProxyServer::HTTPS;
        else if (CFEqual(typeString, kCFProxyTypeSOCKS))
            type = ProxyServer::SOCKS;
        else {
            // We don't know how to handle this type.
            continue;
        }

        CFStringRef host = static_cast<CFStringRef>(CFDictionaryGetValue(proxyDictionary, kCFProxyHostNameKey));
        CFNumberRef port = static_cast<CFNumberRef>(CFDictionaryGetValue(proxyDictionary, kCFProxyPortNumberKey));
        SInt32 portValue;
        CFNumberGetValue(port, kCFNumberSInt32Type, &portValue);

        proxyServers.append(ProxyServer(type, host, portValue));
    }
}

Vector<ProxyServer> proxyServersForURL(const KURL& url)
{
    Vector<ProxyServer> proxyServers;
    
    addProxyServersForURL(proxyServers, url);
    return proxyServers;
    
}
#else
Vector<ProxyServer> proxyServersForURL(const KURL&)
{
    // FIXME: Implement.
    return Vector<ProxyServer>();
}
#endif

} // namespace WebCore
