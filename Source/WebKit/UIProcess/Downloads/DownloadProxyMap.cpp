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

#include "config.h"
#include "DownloadProxyMap.h"

#include "AuxiliaryProcessProxy.h"
#include "DownloadProxy.h"
#include "DownloadProxyMessages.h"
#include "Logging.h"
#include "MessageReceiverMap.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#include "ProcessAssertion.h"
#include "WebProcessPool.h"
#include <wtf/StdLibExtras.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/Entitlements.h>
#endif

namespace WebKit {

DownloadProxyMap::DownloadProxyMap(NetworkProcessProxy& process)
    : m_process(makeWeakPtr(process))
#if PLATFORM(COCOA)
    , m_shouldTakeAssertion(WTF::processHasEntitlement("com.apple.multitasking.systemappassertions"))
#endif
{
    platformCreate();
}

DownloadProxyMap::~DownloadProxyMap()
{
    ASSERT(m_downloads.isEmpty());
    platformDestroy();
}

#if !PLATFORM(COCOA)
void DownloadProxyMap::platformCreate()
{
}

void DownloadProxyMap::platformDestroy()
{
}
#endif

void DownloadProxyMap::applicationDidEnterBackground()
{
    if (m_process)
        m_process->send(Messages::NetworkProcess::ApplicationDidEnterBackground(), 0);
}

void DownloadProxyMap::applicationWillEnterForeground()
{
    if (m_process)
        m_process->send(Messages::NetworkProcess::ApplicationWillEnterForeground(), 0);
}

DownloadProxy& DownloadProxyMap::createDownloadProxy(WebsiteDataStore& dataStore, WebProcessPool& processPool, const WebCore::ResourceRequest& resourceRequest)
{
    auto downloadProxy = DownloadProxy::create(*this, dataStore, processPool, resourceRequest);
    m_downloads.set(downloadProxy->downloadID(), downloadProxy.copyRef());

    RELEASE_LOG(Loading, "Adding download %" PRIu64 " to UIProcess DownloadProxyMap", downloadProxy->downloadID().downloadID());

    if (m_downloads.size() == 1 && m_shouldTakeAssertion) {
        ASSERT(!m_downloadUIAssertion);
        m_downloadUIAssertion = makeUnique<ProcessAssertion>(getCurrentProcessID(), "WebKit downloads"_s, AssertionState::UnboundedNetworking);

        ASSERT(!m_downloadNetworkingAssertion);
        RELEASE_ASSERT(m_process);
        m_downloadNetworkingAssertion = makeUnique<ProcessAssertion>(m_process->processIdentifier(), "WebKit downloads"_s, AssertionState::UnboundedNetworking);

        RELEASE_LOG(ProcessSuspension, "UIProcess took 'WebKit downloads' assertions for UIProcess and NetworkProcess");
    }

    m_process->addMessageReceiver(Messages::DownloadProxy::messageReceiverName(), downloadProxy->downloadID().downloadID(), downloadProxy.get());

    return downloadProxy;
}

void DownloadProxyMap::downloadFinished(DownloadProxy& downloadProxy)
{
    auto downloadID = downloadProxy.downloadID();

    RELEASE_LOG(Loading, "Removing download %" PRIu64 " from UIProcess DownloadProxyMap", downloadID.downloadID());

    // The DownloadProxy may be holding the last reference to the process pool.
    auto protectedProcessPool = makeRefPtr(m_process->processPool());

    ASSERT(m_downloads.contains(downloadID));

    m_process->removeMessageReceiver(Messages::DownloadProxy::messageReceiverName(), downloadID.downloadID());
    downloadProxy.invalidate();
    m_downloads.remove(downloadID);

    if (m_downloads.isEmpty() && m_shouldTakeAssertion) {
        ASSERT(m_downloadUIAssertion);
        ASSERT(m_downloadNetworkingAssertion);
        m_downloadUIAssertion = nullptr;
        m_downloadNetworkingAssertion = nullptr;
        RELEASE_LOG(ProcessSuspension, "UIProcess released 'WebKit downloads' assertions for UIProcess and NetworkProcess");
    }
}

void DownloadProxyMap::invalidate()
{
    // Invalidate all outstanding downloads.
    for (const auto& download : m_downloads.values()) {
        download->processDidClose();
        download->invalidate();
        m_process->removeMessageReceiver(Messages::DownloadProxy::messageReceiverName(), download->downloadID().downloadID());
    }

    m_downloads.clear();
    m_downloadUIAssertion = nullptr;
    m_downloadNetworkingAssertion = nullptr;
    RELEASE_LOG(ProcessSuspension, "UIProcess DownloadProxyMap invalidated - Released 'WebKit downloads' assertions for UIProcess and NetworkProcess");

    m_process = nullptr;
}

} // namespace WebKit
