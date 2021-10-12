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

#if PLATFORM(IOS_FAMILY)

#import "NetworkCache.h"
#import "NetworkProcessCreationParameters.h"
#import "ProcessAssertion.h"
#import "SandboxInitializationParameters.h"
#import "SecItemShim.h"
#import <WebCore/CertificateInfo.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/WebCoreThreadSystemInterface.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/cocoa/Entitlements.h>

namespace WebKit {

#if !PLATFORM(MACCATALYST)

void NetworkProcess::initializeProcess(const AuxiliaryProcessInitializationParameters&)
{
    InitWebCoreThreadSystemInterface();
}

void NetworkProcess::initializeProcessName(const AuxiliaryProcessInitializationParameters&)
{
    notImplemented();
}

void NetworkProcess::initializeSandbox(const AuxiliaryProcessInitializationParameters&, SandboxInitializationParameters&)
{
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

static bool disableServiceWorkerEntitlementTestingOverride;

bool NetworkProcess::parentProcessHasServiceWorkerEntitlement() const
{
    if (disableServiceWorkerEntitlementTestingOverride)
        return false;

    static bool hasEntitlement = WTF::hasEntitlement(parentProcessConnection()->xpcConnection(), "com.apple.developer.WebKit.ServiceWorkers") || WTF::hasEntitlement(parentProcessConnection()->xpcConnection(), "com.apple.developer.web-browser");
    return hasEntitlement;
}

void NetworkProcess::disableServiceWorkerEntitlement()
{
    disableServiceWorkerEntitlementTestingOverride = true;
}

void NetworkProcess::clearServiceWorkerEntitlementOverride(CompletionHandler<void()>&& completionHandler)
{
    disableServiceWorkerEntitlementTestingOverride = false;
    completionHandler();
}

#endif // !PLATFORM(MACCATALYST)

void NetworkProcess::setIsHoldingLockedFiles(bool isHoldingLockedFiles)
{
    if (!isHoldingLockedFiles) {
        m_holdingLockedFileAssertion = nullptr;
        return;
    }

    if (m_holdingLockedFileAssertion && m_holdingLockedFileAssertion->isValid())
        return;

    // We synchronously take a process assertion when beginning a SQLite transaction so that we don't get suspended
    // while holding a locked file. We would get killed if suspended while holding locked files.
    m_holdingLockedFileAssertion = ProcessAssertion::create(getCurrentProcessID(), "Network Process is holding locked files"_s, ProcessAssertionType::FinishTaskInterruptable, ProcessAssertion::Mode::Sync);
    m_holdingLockedFileAssertion->setPrepareForInvalidationHandler([this, weakThis = WeakPtr { *this }]() mutable {
        ASSERT(isMainRunLoop());
        if (!weakThis)
            return;

        if (!m_shouldSuspendIDBServers)
            return;

        for (auto& server : m_webIDBServers.values())
            server->suspend();
    });
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
