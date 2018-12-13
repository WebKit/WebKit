/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC) || PLATFORM(IOSMAC)

#import "NetworkCache.h"
#import "NetworkProcessCreationParameters.h"
#import "NetworkResourceLoader.h"
#import "ResourceCachesToClear.h"
#import "SandboxExtension.h"
#import "SandboxInitializationParameters.h"
#import "SecItemShim.h"
#import "StringUtilities.h"
#import "WKFoundation.h"
#import <WebCore/CertificateInfo.h>
#import <WebCore/FileSystem.h>
#import <WebCore/LocalizedStrings.h>
#import <notify.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <sysexits.h>
#import <wtf/MemoryPressureHandler.h>
#import <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

void NetworkProcess::initializeProcess(const ChildProcessInitializationParameters&)
{
    // Having a window server connection in this process would result in spin logs (<rdar://problem/13239119>).
    setApplicationIsDaemon();
}

void NetworkProcess::initializeProcessName(const ChildProcessInitializationParameters& parameters)
{
#if !PLATFORM(IOSMAC)
    NSString *applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Networking", "visible name of the network process. The argument is the application name."), (NSString *)parameters.uiProcessName];
    _LSSetApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), _kLSDisplayNameKey, (CFStringRef)applicationName, nullptr);
#endif
}

static void overrideSystemProxies(const String& httpProxy, const String& httpsProxy)
{
    NSMutableDictionary *proxySettings = [NSMutableDictionary dictionary];

    if (!httpProxy.isNull()) {
        URL httpProxyURL(URL(), httpProxy);
        if (httpProxyURL.isValid()) {
            [proxySettings setObject:nsStringFromWebCoreString(httpProxyURL.host().toString()) forKey:(NSString *)kCFNetworkProxiesHTTPProxy];
            if (httpProxyURL.port()) {
                NSNumber *port = [NSNumber numberWithInt:httpProxyURL.port().value()];
                [proxySettings setObject:port forKey:(NSString *)kCFNetworkProxiesHTTPPort];
            }
        }
        else
            NSLog(@"Malformed HTTP Proxy URL '%s'.  Expected 'http://<hostname>[:<port>]'\n", httpProxy.utf8().data());
    }

    if (!httpsProxy.isNull()) {
        URL httpsProxyURL(URL(), httpsProxy);
        if (httpsProxyURL.isValid()) {
#if !PLATFORM(IOSMAC)
            [proxySettings setObject:nsStringFromWebCoreString(httpsProxyURL.host().toString()) forKey:(NSString *)kCFNetworkProxiesHTTPSProxy];
            if (httpsProxyURL.port()) {
                NSNumber *port = [NSNumber numberWithInt:httpsProxyURL.port().value()];
                [proxySettings setObject:port forKey:(NSString *)kCFNetworkProxiesHTTPSPort];
            }
#endif
        } else
            NSLog(@"Malformed HTTPS Proxy URL '%s'.  Expected 'https://<hostname>[:<port>]'\n", httpsProxy.utf8().data());
    }

    if ([proxySettings count] > 0)
        _CFNetworkSetOverrideSystemProxySettings((__bridge CFDictionaryRef)proxySettings);
}

void NetworkProcess::platformInitializeNetworkProcess(const NetworkProcessCreationParameters& parameters)
{
    platformInitializeNetworkProcessCocoa(parameters);

#if ENABLE(SEC_ITEM_SHIM)
    // SecItemShim is needed for CFNetwork APIs that query Keychains beneath us.
    initializeSecItemShim(*this);
#endif

    if (!parameters.httpProxy.isNull() || !parameters.httpsProxy.isNull())
        overrideSystemProxies(parameters.httpProxy, parameters.httpsProxy);
}

void NetworkProcess::allowSpecificHTTPSCertificateForHost(const CertificateInfo& certificateInfo, const String& host)
{
    [NSURLRequest setAllowsSpecificHTTPSCertificate:(__bridge NSArray *)certificateInfo.certificateChain() forHost:(NSString *)host];
}

void NetworkProcess::initializeSandbox(const ChildProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
    // Need to overide the default, because service has a different bundle ID.
#if WK_API_ENABLED
    NSBundle *webKit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKWebView")];
#else
    NSBundle *webKit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKView")];
#endif

    sandboxParameters.setOverrideSandboxProfilePath([webKit2Bundle pathForResource:@"com.apple.WebKit.NetworkProcess" ofType:@"sb"]);

    ChildProcess::initializeSandbox(parameters, sandboxParameters);
}

void NetworkProcess::clearCacheForAllOrigins(uint32_t cachesToClear)
{
    ResourceCachesToClear resourceCachesToClear = static_cast<ResourceCachesToClear>(cachesToClear);
    if (resourceCachesToClear == InMemoryResourceCachesOnly)
        return;

    clearDiskCache(-WallTime::infinity(), [] { });
}

void NetworkProcess::platformTerminate()
{
    if (m_clearCacheDispatchGroup) {
        dispatch_group_wait(m_clearCacheDispatchGroup, DISPATCH_TIME_FOREVER);
        dispatch_release(m_clearCacheDispatchGroup);
        m_clearCacheDispatchGroup = 0;
    }
}

#if PLATFORM(IOSMAC)
bool NetworkProcess::parentProcessHasServiceWorkerEntitlement() const
{
    return true;
}
#endif

} // namespace WebKit

#endif // PLATFORM(MAC)
