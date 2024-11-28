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

#include "APIDownloadClient.h"
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
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/Entitlements.h>
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DownloadProxyMap);

DownloadProxyMap::DownloadProxyMap(NetworkProcessProxy& process)
    : m_process(process)
#if PLATFORM(COCOA)
    , m_shouldTakeAssertion(WTF::processHasEntitlement("com.apple.multitasking.systemappassertions"_s))
#endif
{
    platformCreate();
}

DownloadProxyMap::~DownloadProxyMap()
{
    ASSERT(m_downloads.isEmpty());
    platformDestroy();
}

void DownloadProxyMap::ref() const
{
    m_process->ref();
}

void DownloadProxyMap::deref() const
{
    m_process->deref();
}

Ref<NetworkProcessProxy> DownloadProxyMap::protectedProcess()
{
    return m_process.get();
}

#if !PLATFORM(COCOA)
void DownloadProxyMap::platformCreate()
{
}

void DownloadProxyMap::platformDestroy()
{
}
#endif

Ref<DownloadProxy> DownloadProxyMap::createDownloadProxy(WebsiteDataStore& dataStore, Ref<API::DownloadClient>&& client, const WebCore::ResourceRequest& resourceRequest, const FrameInfoData& frameInfo, WebPageProxy* originatingPage)
{
    auto downloadProxy = DownloadProxy::create(*this, dataStore, WTFMove(client), resourceRequest, frameInfo, originatingPage);
    m_downloads.set(downloadProxy->downloadID(), downloadProxy.copyRef());

    RELEASE_LOG(Loading, "Adding download %" PRIu64 " to UIProcess DownloadProxyMap", downloadProxy->downloadID().toUInt64());

    if (m_downloads.size() == 1 && m_shouldTakeAssertion) {
        ASSERT(!m_downloadUIAssertion);
        m_downloadUIAssertion = ProcessAssertion::create(getCurrentProcessID(), "WebKit downloads"_s, ProcessAssertionType::UnboundedNetworking);
        RELEASE_LOG(ProcessSuspension, "UIProcess took 'WebKit downloads' assertion for UIProcess");
    }

    protectedProcess()->addMessageReceiver(Messages::DownloadProxy::messageReceiverName(), downloadProxy->downloadID().toUInt64(), downloadProxy.get());

    return downloadProxy;
}

void DownloadProxyMap::downloadFinished(DownloadProxy& downloadProxy)
{
    auto downloadID = downloadProxy.downloadID();

    RELEASE_LOG(Loading, "Removing download %" PRIu64 " from UIProcess DownloadProxyMap", downloadID.toUInt64());

    ASSERT(m_downloads.contains(downloadID));

    protectedProcess()->removeMessageReceiver(Messages::DownloadProxy::messageReceiverName(), downloadID.toUInt64());
    downloadProxy.invalidate();
    m_downloads.remove(downloadID);

    if (m_downloads.isEmpty() && m_shouldTakeAssertion) {
        ASSERT(m_downloadUIAssertion);
        m_downloadUIAssertion = nullptr;
        RELEASE_LOG(ProcessSuspension, "UIProcess released 'WebKit downloads' assertion for UIProcess");
    }
}

void DownloadProxyMap::invalidate()
{
    // Invalidate all outstanding downloads.
    for (const auto& download : m_downloads.values()) {
        download->processDidClose();
        download->invalidate();
        protectedProcess()->removeMessageReceiver(Messages::DownloadProxy::messageReceiverName(), download->downloadID().toUInt64());
    }

    m_downloads.clear();
    m_downloadUIAssertion = nullptr;
    RELEASE_LOG(ProcessSuspension, "UIProcess DownloadProxyMap invalidated - Released 'WebKit downloads' assertion for UIProcess");
}

} // namespace WebKit
