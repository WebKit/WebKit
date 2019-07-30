/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#import "config.h"
#import "NetworkProcess.h"

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)

#import "NetworkCache.h"
#import "NetworkProcessCreationParameters.h"
#import "ResourceCachesToClear.h"
#import "SandboxInitializationParameters.h"
#import "SecItemShim.h"
#import <WebCore/CertificateInfo.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/WebCoreThreadSystemInterface.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/cocoa/Entitlements.h>

#define ENABLE_MANUAL_NETWORK_SANDBOXING 0

namespace WebKit {
using namespace WebCore;

void NetworkProcess::initializeProcess(const AuxiliaryProcessInitializationParameters&)
{
    InitWebCoreThreadSystemInterface();
}

void NetworkProcess::initializeProcessName(const AuxiliaryProcessInitializationParameters&)
{
    notImplemented();
}

void NetworkProcess::initializeSandbox(const AuxiliaryProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
#if ENABLE_MANUAL_NETWORK_SANDBOXING
    // Need to override the default, because service has a different bundle ID.
    NSBundle *webkit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKWebView")];
    sandboxParameters.setOverrideSandboxProfilePath([webkit2Bundle pathForResource:@"com.apple.WebKit.NetworkProcess" ofType:@"sb"]);

    AuxiliaryProcess::initializeSandbox(parameters, sandboxParameters);
#else
    UNUSED_PARAM(parameters);
    UNUSED_PARAM(sandboxParameters);
#endif
}

void NetworkProcess::allowSpecificHTTPSCertificateForHost(const CertificateInfo& certificateInfo, const String& host)
{
    [NSURLRequest setAllowsSpecificHTTPSCertificate:(NSArray *)certificateInfo.certificateChain() forHost:host];
}

void NetworkProcess::clearCacheForAllOrigins(uint32_t cachesToClear)
{
    ResourceCachesToClear resourceCachesToClear = static_cast<ResourceCachesToClear>(cachesToClear);
    if (resourceCachesToClear == InMemoryResourceCachesOnly)
        return;
    forEachNetworkSession([](NetworkSession& session) {
        if (auto* cache = session.cache())
            cache->clear();
    });
}

void NetworkProcess::platformInitializeNetworkProcess(const NetworkProcessCreationParameters& parameters)
{
#if ENABLE(SEC_ITEM_SHIM)
    // SecItemShim is needed for CFNetwork APIs that query Keychains beneath us.
    initializeSecItemShim(*this);
#endif
    platformInitializeNetworkProcessCocoa(parameters);
}

void NetworkProcess::platformTerminate()
{
    notImplemented();
}

bool NetworkProcess::parentProcessHasServiceWorkerEntitlement() const
{
    static bool hasEntitlement = WTF::hasEntitlement(parentProcessConnection()->xpcConnection(), "com.apple.developer.WebKit.ServiceWorkers");
    return hasEntitlement;
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
