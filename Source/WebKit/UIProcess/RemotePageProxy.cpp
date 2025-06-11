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
#include "RemotePageProxy.h"

#include "APIWebsitePolicies.h"
#include "DrawingAreaProxy.h"
#include "FormDataReference.h"
#include "FrameInfoData.h"
#include "HandleMessage.h"
#include "NativeWebMouseEvent.h"
#include "NavigationActionData.h"
#include "PageLoadState.h"
#include "ProvisionalFrameProxy.h"
#include "ProvisionalPageProxy.h"
#include "RemotePageDrawingAreaProxy.h"
#include "RemotePageFullscreenManagerProxy.h"
#include "RemotePageVisitedLinkStoreRegistration.h"
#include "UserMediaProcessManager.h"
#include "WebFrameProxy.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebProcessActivityState.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include <WebCore/MediaProducer.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/RemoteUserInputEventData.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(FULLSCREEN_API)
#include "WebFullScreenManagerProxy.h"
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemotePageProxy);

Ref<RemotePageProxy> RemotePageProxy::create(WebPageProxy& page, WebCore::PageIdentifier trackingWebPageID, WebProcessProxy& process, const WebCore::Site& site, WebPageProxyMessageReceiverRegistration* registrationToTransfer, std::optional<WebCore::PageIdentifier> pageIDToTransfer)
{
    return adoptRef(*new RemotePageProxy(page, trackingWebPageID, process, site, registrationToTransfer, pageIDToTransfer));
}

RemotePageProxy::RemotePageProxy(WebPageProxy& page, WebCore::PageIdentifier trackingWebPageID, WebProcessProxy& process, const WebCore::Site& site, WebPageProxyMessageReceiverRegistration* registrationToTransfer, std::optional<WebCore::PageIdentifier> pageIDToTransfer)
    : m_webPageID(pageIDToTransfer.value_or(WebCore::PageIdentifier::generate()))
    , m_process(process)
    , m_page(page)
    , m_trackingWebPageID(trackingWebPageID)
    , m_site(site)
    , m_processActivityState(makeUniqueRef<WebProcessActivityState>(*this))
{
    if (registrationToTransfer)
        m_messageReceiverRegistration.transferMessageReceivingFrom(*registrationToTransfer, *this);
    else
        m_messageReceiverRegistration.startReceivingMessages(m_process, m_webPageID, *this);

    m_process->addRemotePageProxy(*this);
}

void RemotePageProxy::injectPageIntoNewProcess()
{
    RefPtr page = m_page.get();
    if (!page) {
        ASSERT_NOT_REACHED();
        return;
    }
    if (!page->mainFrame()) {
        ASSERT_NOT_REACHED();
        return;
    }

    CheckedPtr drawingArea = page->drawingArea();
    RELEASE_ASSERT(drawingArea);

    m_drawingArea = RemotePageDrawingAreaProxy::create(*drawingArea, m_process);
#if ENABLE(FULLSCREEN_API)
    m_fullscreenManager = RemotePageFullscreenManagerProxy::create(pageID(), page->protectedFullScreenManager().get(), m_process);
#endif
    m_visitedLinkStoreRegistration = makeUnique<RemotePageVisitedLinkStoreRegistration>(*page, m_process);

    m_process->send(
        Messages::WebProcess::CreateWebPage(
            m_webPageID,
            page->creationParametersForRemotePage(m_process, *drawingArea, RemotePageParameters {
                URL(page->pageLoadState().url()),
                page->protectedMainFrame()->frameTreeCreationParameters(),
                page->mainFrameWebsitePoliciesData() ? std::make_optional(*page->mainFrameWebsitePoliciesData()) : std::nullopt
            })
        ), 0
    );
}

void RemotePageProxy::injectProvisionalPageIntoProcess(ProvisionalPageProxy& provisionalPage)
{
    RefPtr page = m_page.get();
    if (!page) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr provisionalMainFrame = provisionalPage.mainFrame();
    if (!provisionalMainFrame)
        return;

    CheckedPtr provisionalDrawingArea = provisionalPage.drawingArea();
    if (!provisionalDrawingArea)
        return;

    if (m_drawingArea && m_drawingArea->identifier() == provisionalDrawingArea->identifier())
        return;

    m_drawingArea = RemotePageDrawingAreaProxy::create(*provisionalDrawingArea, m_process);
#if ENABLE(FULLSCREEN_API)
    m_fullscreenManager = RemotePageFullscreenManagerProxy::create(m_webPageID, page->protectedFullScreenManager().get(), m_process);
#endif
    m_visitedLinkStoreRegistration = makeUnique<RemotePageVisitedLinkStoreRegistration>(*page, m_process);

    RemotePageParameters remotePageParameters {
        provisionalPage.requestURL(),
        provisionalMainFrame->frameTreeCreationParameters(),
        provisionalPage.mainFrameWebsitePoliciesData() ? std::make_optional(*provisionalPage.mainFrameWebsitePoliciesData()) : std::nullopt
    };
    m_process->send(
        Messages::WebProcess::CreateWebPage(
            m_webPageID,
            page->creationParameters(m_process, *provisionalDrawingArea, provisionalMainFrame->frameID(), WTFMove(remotePageParameters), true)
        ), 0
    );
}

void RemotePageProxy::processDidTerminate(WebProcessProxy& process, ProcessTerminationReason reason)
{
    RefPtr page = m_page.get();
    if (!page)
        return;
    if (CheckedPtr drawingArea = page->drawingArea())
        drawingArea->remotePageProcessDidTerminate(process.coreProcessIdentifier());
    if (RefPtr mainFrame = page->mainFrame())
        mainFrame->remoteProcessDidTerminate(process, WebFrameProxy::ClearFrameTreeSyncData::Yes);
    page->dispatchProcessDidTerminate(process, reason);
}

RemotePageProxy::~RemotePageProxy()
{
    if (RefPtr page = m_page.get())
        page->isNoLongerAssociatedWithRemotePage(*this);
    if (m_drawingArea)
        m_process->send(Messages::WebPage::Close(), m_webPageID);
    m_process->removeRemotePageProxy(*this);
}

void RemotePageProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageName() == Messages::WebPageProxy::IsPlayingMediaDidChange::name()) {
        IPC::handleMessage<Messages::WebPageProxy::IsPlayingMediaDidChange>(connection, decoder, this, &RemotePageProxy::isPlayingMediaDidChange);
        return;
    }

    if (RefPtr page = m_page.get())
        page->didReceiveMessage(connection, decoder);
}

bool RemotePageProxy::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder)
{
    if (RefPtr page = m_page.get())
        return page->didReceiveSyncMessage(connection, decoder, encoder);
    return false;
}

Ref<WebProcessProxy> RemotePageProxy::protectedProcess() const
{
    return m_process;
}

RefPtr<WebPageProxy> RemotePageProxy::protectedPage() const
{
    return m_page.get();
}

WebPageProxy* RemotePageProxy::page() const
{
    return m_page.get();
}

WebProcessActivityState& RemotePageProxy::processActivityState()
{
    return m_processActivityState;
}

void RemotePageProxy::isPlayingMediaDidChange(WebCore::MediaProducerMediaStateFlags newState)
{
#if ENABLE(MEDIA_STREAM)
    bool didStopAudioCapture = m_mediaState.containsAny(WebCore::MediaProducer::IsCapturingAudioMask) && !newState.containsAny(WebCore::MediaProducer::IsCapturingAudioMask);
    bool didStopVideoCapture = m_mediaState.containsAny(WebCore::MediaProducer::IsCapturingVideoMask) && !newState.containsAny(WebCore::MediaProducer::IsCapturingVideoMask);
#endif

    m_mediaState = newState;

    RefPtr page = m_page.get();
    if (!page || page->isClosed())
        return;

    page->updatePlayingMediaDidChange(WebPageProxy::CanDelayNotification::Yes);

#if ENABLE(MEDIA_STREAM)
    if (didStopAudioCapture || didStopVideoCapture)
        UserMediaProcessManager::singleton().revokeSandboxExtensionsIfNeeded(protectedProcess().get());
#endif
}

}
