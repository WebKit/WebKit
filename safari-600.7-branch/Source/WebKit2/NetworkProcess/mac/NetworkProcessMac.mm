/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC) && ENABLE(NETWORK_PROCESS)

#import "NetworkProcessCreationParameters.h"
#import "NetworkResourceLoader.h"
#import "ResourceCachesToClear.h"
#import "SandboxExtension.h"
#import "SandboxInitializationParameters.h"
#import "SecItemShim.h"
#import "StringUtilities.h"
#import <WebCore/CertificateInfo.h>
#import <WebCore/FileSystem.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MemoryPressureHandler.h>
#import <WebKitSystemInterface.h>
#import <notify.h>
#import <sysexits.h>
#import <wtf/text/WTFString.h>

using namespace WebCore;

@interface NSURLRequest (Details) 
+ (void)setAllowsSpecificHTTPSCertificate:(NSArray *)allow forHost:(NSString *)host;
@end

namespace WebKit {

void NetworkProcess::initializeProcess(const ChildProcessInitializationParameters&)
{
    // Having a window server connection in this process would result in spin logs (<rdar://problem/13239119>).
    setApplicationIsDaemon();
}

void NetworkProcess::initializeProcessName(const ChildProcessInitializationParameters& parameters)
{
    NSString *applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Networking", "visible name of the network process. The argument is the application name."), (NSString *)parameters.uiProcessName];
    WKSetVisibleApplicationName((CFStringRef)applicationName);
}

static void overrideSystemProxies(const String& httpProxy, const String& httpsProxy)
{
    NSMutableDictionary *proxySettings = [NSMutableDictionary dictionary];

    if (!httpProxy.isNull()) {
        URL httpProxyURL(URL(), httpProxy);
        if (httpProxyURL.isValid()) {
            [proxySettings setObject:nsStringFromWebCoreString(httpProxyURL.host()) forKey:(NSString *)kCFNetworkProxiesHTTPProxy];
            if (httpProxyURL.hasPort()) {
                NSNumber *port = [NSNumber numberWithInt:httpProxyURL.port()];
                [proxySettings setObject:port forKey:(NSString *)kCFNetworkProxiesHTTPPort];
            }
        }
        else
            NSLog(@"Malformed HTTP Proxy URL '%s'.  Expected 'http://<hostname>[:<port>]'\n", httpProxy.utf8().data());
    }

    if (!httpsProxy.isNull()) {
        URL httpsProxyURL(URL(), httpsProxy);
        if (httpsProxyURL.isValid()) {
            [proxySettings setObject:nsStringFromWebCoreString(httpsProxyURL.host()) forKey:(NSString *)kCFNetworkProxiesHTTPSProxy];
            if (httpsProxyURL.hasPort()) {
                NSNumber *port = [NSNumber numberWithInt:httpsProxyURL.port()];
                [proxySettings setObject:port forKey:(NSString *)kCFNetworkProxiesHTTPSPort];
            }
        } else
            NSLog(@"Malformed HTTPS Proxy URL '%s'.  Expected 'https://<hostname>[:<port>]'\n", httpsProxy.utf8().data());
    }

    if ([proxySettings count] > 0)
        WKCFNetworkSetOverrideSystemProxySettings((CFDictionaryRef)proxySettings);
}

void NetworkProcess::platformInitializeNetworkProcess(const NetworkProcessCreationParameters& parameters)
{
    platformInitializeNetworkProcessCocoa(parameters);

#if ENABLE(SEC_ITEM_SHIM)
    SecItemShim::shared().initialize(this);
#endif

    if (!parameters.httpProxy.isNull() || !parameters.httpsProxy.isNull())
        overrideSystemProxies(parameters.httpProxy, parameters.httpsProxy);
}

void NetworkProcess::allowSpecificHTTPSCertificateForHost(const CertificateInfo& certificateInfo, const String& host)
{
    [NSURLRequest setAllowsSpecificHTTPSCertificate:(NSArray *)certificateInfo.certificateChain() forHost:(NSString *)host];
}

void NetworkProcess::initializeSandbox(const ChildProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
    // Need to overide the default, because service has a different bundle ID.
    NSBundle *webkit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKView")];
    sandboxParameters.setOverrideSandboxProfilePath([webkit2Bundle pathForResource:@"com.apple.WebKit.NetworkProcess" ofType:@"sb"]);

    ChildProcess::initializeSandbox(parameters, sandboxParameters);
}

void NetworkProcess::clearCacheForAllOrigins(uint32_t cachesToClear)
{
    ResourceCachesToClear resourceCachesToClear = static_cast<ResourceCachesToClear>(cachesToClear);
    if (resourceCachesToClear == InMemoryResourceCachesOnly)
        return;

    if (!m_clearCacheDispatchGroup)
        m_clearCacheDispatchGroup = dispatch_group_create();

    dispatch_group_async(m_clearCacheDispatchGroup, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [[NSURLCache sharedURLCache] removeAllCachedResponses];
    });
}

void NetworkProcess::platformTerminate()
{
    if (m_clearCacheDispatchGroup) {
        dispatch_group_wait(m_clearCacheDispatchGroup, DISPATCH_TIME_FOREVER);
        dispatch_release(m_clearCacheDispatchGroup);
        m_clearCacheDispatchGroup = 0;
    }
}

} // namespace WebKit

#endif // PLATFORM(MAC) && ENABLE(NETWORK_PROCESS)
