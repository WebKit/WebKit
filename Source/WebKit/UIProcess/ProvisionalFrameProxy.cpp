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

#include "APIWebsitePolicies.h"
#include "DrawingAreaProxy.h"
#include "FrameInfoData.h"
#include "HandleMessage.h"
#include "LoadParameters.h"
#include "LoadedWebArchive.h"
#include "LocalFrameCreationParameters.h"
#include "MessageSenderInlines.h"
#include "NetworkProcessMessages.h"
#include "RemotePageProxy.h"
#include "VisitedLinkStore.h"
#include "WebFrameProxy.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"

#include <WebCore/FrameIdentifier.h>
#include <WebCore/ShouldTreatAsContinuingLoad.h>

namespace WebKit {

ProvisionalFrameProxy::ProvisionalFrameProxy(WebFrameProxy& frame, Ref<WebProcessProxy>&& process, const WebCore::ResourceRequest& request)
    : m_frame(frame)
    , m_process(WTFMove(process))
    , m_visitedLinkStore(frame.page()->visitedLinkStore())
    , m_pageID(frame.page()->webPageID()) // FIXME: Generate a new one? This can conflict. And we probably want something like ProvisionalPageProxy to respond to messages anyways.
    , m_webPageID(frame.page()->identifier())
    , m_layerHostingContextIdentifier(WebCore::LayerHostingContextIdentifier::generate())
{
    m_process->markProcessAsRecentlyUsed();
    m_process->addProvisionalFrameProxy(*this);

    ASSERT(frame.page());

    LoadParameters loadParameters;
    loadParameters.request = request;
    loadParameters.shouldTreatAsContinuingLoad = WebCore::ShouldTreatAsContinuingLoad::YesAfterNavigationPolicyDecision;
    loadParameters.frameIdentifier = frame.frameID();
    // FIXME: Add more parameters as appropriate.

    LocalFrameCreationParameters localFrameCreationParameters {
        m_layerHostingContextIdentifier
    };

    // FIXME: This gives too much cookie access. This should be removed after putting the entire frame tree in all web processes.
    auto giveAllCookieAccess = LoadedWebArchive::Yes;
    WebCore::RegistrableDomain domain { request.url() };
    m_process->addAllowedFirstPartyForCookies(domain);
    frame.page()->websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::AddAllowedFirstPartyForCookies(m_process->coreProcessIdentifier(), domain, giveAllCookieAccess), [process = m_process, loadParameters = WTFMove(loadParameters), localFrameCreationParameters = WTFMove(localFrameCreationParameters), pageID = m_pageID] () mutable {
        process->send(Messages::WebPage::TransitionFrameToLocal(localFrameCreationParameters, *loadParameters.frameIdentifier), pageID);
        process->send(Messages::WebPage::LoadRequest(loadParameters), pageID);
    });
}

ProvisionalFrameProxy::~ProvisionalFrameProxy()
{
    m_process->removeVisitedLinkStoreUser(m_visitedLinkStore.get(), m_webPageID);
    m_process->removeProvisionalFrameProxy(*this);
}

}
