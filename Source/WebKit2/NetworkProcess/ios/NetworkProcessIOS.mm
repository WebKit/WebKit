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

#if PLATFORM(IOS) && ENABLE(NETWORK_PROCESS)

#import "NetworkProcessCreationParameters.h"
#import "SandboxInitializationParameters.h"
#import "SecItemShim.h"
#import <WebCore/CertificateInfo.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/WebCoreThreadSystemInterface.h>

#define ENABLE_MANUAL_NETWORK_SANDBOXING 0

@interface NSURLRequest (WKDetails)
+ (void)setAllowsSpecificHTTPSCertificate:(NSArray *)certificateChain forHost:(NSString *)host;
@end

using namespace WebCore;

namespace WebKit {

void NetworkProcess::initializeProcess(const ChildProcessInitializationParameters&)
{
    InitWebCoreThreadSystemInterface();
}

void NetworkProcess::initializeProcessName(const ChildProcessInitializationParameters&)
{
    notImplemented();
}

void NetworkProcess::initializeSandbox(const ChildProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
#if ENABLE_MANUAL_NETWORK_SANDBOXING
    // Need to override the default, because service has a different bundle ID.
    NSBundle *webkit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKView")];
    sandboxParameters.setOverrideSandboxProfilePath([webkit2Bundle pathForResource:@"com.apple.WebKit.NetworkProcess" ofType:@"sb"]);

    ChildProcess::initializeSandbox(parameters, sandboxParameters);
#else
    UNUSED_PARAM(parameters);
    UNUSED_PARAM(sandboxParameters);
#endif
}

void NetworkProcess::allowSpecificHTTPSCertificateForHost(const CertificateInfo& certificateInfo, const String& host)
{
    [NSURLRequest setAllowsSpecificHTTPSCertificate:(NSArray *)certificateInfo.certificateChain() forHost:host];
}

void NetworkProcess::clearCacheForAllOrigins(uint32_t)
{
}

void NetworkProcess::platformInitializeNetworkProcess(const NetworkProcessCreationParameters& parameters)
{
#if ENABLE(SEC_ITEM_SHIM)
    SecItemShim::shared().initialize(this);
#endif
    platformInitializeNetworkProcessCocoa(parameters);
}

void NetworkProcess::platformTerminate()
{
    notImplemented();
}

} // namespace WebKit

#endif // PLATFORM(IOS) && ENABLE(NETWORK_PROCESS)
