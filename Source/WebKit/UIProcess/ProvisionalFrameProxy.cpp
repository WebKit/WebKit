/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ProvisionalFrameProxy.h"

#include "RemotePageProxy.h"
#include "VisitedLinkStore.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"

namespace WebKit {

ProvisionalFrameProxy::ProvisionalFrameProxy(WebFrameProxy& frame, WebProcessProxy& process, RefPtr<RemotePageProxy>&& remotePageProxy)
    : m_frame(frame)
    , m_process(process)
    , m_remotePageProxy(WTFMove(remotePageProxy))
    , m_visitedLinkStore(frame.page()->visitedLinkStore())
    , m_pageID(frame.page()->webPageID())
    , m_webPageID(frame.page()->identifier())
    , m_layerHostingContextIdentifier(WebCore::LayerHostingContextIdentifier::generate())
{
    ASSERT(!m_remotePageProxy || m_remotePageProxy->process().coreProcessIdentifier() == process.coreProcessIdentifier());
    m_process->markProcessAsRecentlyUsed();
}

ProvisionalFrameProxy::~ProvisionalFrameProxy() = default;

RefPtr<RemotePageProxy> ProvisionalFrameProxy::takeRemotePageProxy()
{
    return std::exchange(m_remotePageProxy, nullptr);
}

Ref<WebProcessProxy> ProvisionalFrameProxy::protectedProcess() const
{
    return process();
}

} // namespace WebKit
