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
#include "MessageReceiverMap.h"
#include "ProcessAssertion.h"
#include <wtf/StdLibExtras.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/Entitlements.h>
#endif

namespace WebKit {

DownloadProxyMap::DownloadProxyMap(NetworkProcessProxy& process)
    : m_process(&process)
#if PLATFORM(COCOA)
    , m_shouldTakeAssertion(WTF::processHasEntitlement("com.apple.multitasking.systemappassertions"))
#endif
{
}

DownloadProxyMap::~DownloadProxyMap()
{
    ASSERT(m_downloads.isEmpty());
}

DownloadProxy* DownloadProxyMap::createDownloadProxy(WebProcessPool& processPool, const WebCore::ResourceRequest& resourceRequest)
{
    auto downloadProxy = DownloadProxy::create(*this, processPool, resourceRequest);
    m_downloads.set(downloadProxy->downloadID(), downloadProxy.copyRef());

    if (m_downloads.size() == 1 && m_shouldTakeAssertion) {
        ASSERT(!m_downloadAssertion);
        m_downloadAssertion = std::make_unique<ProcessAssertion>(getCurrentProcessID(), "WebKit downloads"_s, AssertionState::UnboundedNetworking);
    }

    m_process->addMessageReceiver(Messages::DownloadProxy::messageReceiverName(), downloadProxy->downloadID().downloadID(), downloadProxy.get());

    return downloadProxy.ptr();
}

void DownloadProxyMap::downloadFinished(DownloadProxy* downloadProxy)
{
    auto downloadID = downloadProxy->downloadID();

    ASSERT(m_downloads.contains(downloadID));

    m_process->removeMessageReceiver(Messages::DownloadProxy::messageReceiverName(), downloadID.downloadID());
    downloadProxy->invalidate();
    m_downloads.remove(downloadID);

    if (m_downloads.isEmpty() && m_shouldTakeAssertion) {
        ASSERT(m_downloadAssertion);
        m_downloadAssertion = nullptr;
    }
}

void DownloadProxyMap::processDidClose()
{
    // Invalidate all outstanding downloads.
    for (const auto& download : m_downloads.values()) {
        download->processDidClose();
        download->invalidate();
        m_process->removeMessageReceiver(Messages::DownloadProxy::messageReceiverName(), download->downloadID().downloadID());
    }

    m_downloads.clear();
    m_downloadAssertion = nullptr;
    m_process = nullptr;
}

} // namespace WebKit
