/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#include "RemotePageDrawingAreaProxy.h"
#include "RemotePageFullscreenManagerProxy.h"
#include "RemotePageScreenOrientationManagerProxy.h"
#include "RemotePageVisitedLinkStoreRegistration.h"
#include "RemotePageWebDeviceOrientationUpdateProviderProxy.h"
#include "UserMediaProcessManager.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListMessages.h"
#include "WebFrameProxy.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebProcessActivityState.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include "WebScreenOrientationManagerProxy.h"
#include <WebCore/MediaProducer.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/RemoteUserInputEventData.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(FULLSCREEN_API)
#include "WebFullScreenManagerProxy.h"
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
#include "RemotePageVideoPresentationManagerProxy.h"
#include "VideoPresentationManagerProxy.h"
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
#include "WebDeviceOrientationUpdateProviderProxy.h"
#endif

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=306415
#if ENABLE(BACK_FORWARD_LIST_SWIFT)
#include "WebKit-Swift.h"
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemotePageProxy);

Ref<RemotePageProxy> RemotePageProxy::create(WebPageProxy& page, WebProcessProxy& process, const WebCore::Site& site, WebPageProxyMessageReceiverRegistration* registrationToTransfer, std::optional<WebCore::PageIdentifier> pageIDToTransfer)
{
    return adoptRef(*new RemotePageProxy(page, process, site, registrationToTransfer, pageIDToTransfer));
}

RemotePageProxy::RemotePageProxy(WebPageProxy& page, WebProcessProxy& process, const WebCore::Site& site, WebPageProxyMessageReceiverRegistration* registrationToTransfer, std::optional<WebCore::PageIdentifier> pageIDToTransfer)
    : m_webPageID(pageIDToTransfer.value_or(WebCore::PageIdentifier::generate()))
    , m_process(process)
    , m_page(page)
    , m_site(site)
    , m_processActivityState(makeUniqueRef<WebProcessActivityState>(*this))
{
    if (registrationToTransfer)
        m_messageReceiverRegistration.transferMessageReceivingFrom(*registrationToTransfer, *this, page.backForwardListMessageReceiver());
    else
        m_messageReceiverRegistration.startReceivingMessages(m_process, m_webPageID, *this, page.backForwardListMessageReceiver());

    m_process->addRemotePageProxy(*this);
}

void RemotePageProxy::disconnect()
{
    if (RefPtr page = m_page.get())
        page->isNoLongerAssociatedWithRemotePage(*this);
    if (m_drawingArea)
        m_process->send(Messages::WebPage::Close(), m_webPageID);
    m_process->removeRemotePageProxy(*this);

    m_drawingArea = nullptr;
#if ENABLE(FULLSCREEN_API)
    m_fullscreenManager = nullptr;
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
    m_videoPresentationManager = nullptr;
#endif
#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
    m_webDeviceOrientationUpdateProvider = nullptr;
#endif
#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    m_playbackSessionManager = nullptr;
#endif
    m_visitedLinkStoreRegistration = nullptr;
    m_messageReceiverRegistration.stopReceivingMessages();
    m_screenOrientationManager = nullptr;
#if ASSERT_ENABLED
    m_disconnected = true;
#endif
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

#if PLATFORM(MAC) && USE(RUNNINGBOARD)
    if (page->preferences().backgroundWebContentRunningBoardThrottlingEnabled())
        m_process->setRunningBoardThrottlingEnabled();
#endif

    page->takeActivitiesOnRemotePage(*this);

    Ref drawingArea = *page->drawingArea();
    m_drawingArea = RemotePageDrawingAreaProxy::create(drawingArea.get(), m_process);
#if ENABLE(FULLSCREEN_API)
    m_fullscreenManager = RemotePageFullscreenManagerProxy::create(pageID(), protect(page->fullScreenManager()), m_process);
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
    m_videoPresentationManager = RemotePageVideoPresentationManagerProxy::create(pageID(), m_process, protect(page->videoPresentationManager()));
#endif
#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
    m_webDeviceOrientationUpdateProvider = RemotePageWebDeviceOrientationUpdateProviderProxy::create(pageID(), m_process, protect(page->webDeviceOrientationUpdateProviderProxy()));
#endif
#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    m_playbackSessionManager = RemotePagePlaybackSessionManagerProxy::create(pageID(), protect(page->playbackSessionManager()), m_process);
#endif

    if (RefPtr screenOrientationManager = page->screenOrientationManager())
        m_screenOrientationManager = RemotePageScreenOrientationManagerProxy::create(m_webPageID, screenOrientationManager.get(), m_process);

    m_visitedLinkStoreRegistration = makeUnique<RemotePageVisitedLinkStoreRegistration>(*page, m_process);

    RefPtr websitePolicies = page->mainFrameWebsitePolicies();
    m_process->send(
        Messages::WebProcess::CreateWebPage(
            m_webPageID,
            page->creationParametersForRemotePage(m_process, drawingArea.get(), RemotePageParameters {
                URL(page->pageLoadState().url()),
                protect(page->mainFrame())->frameTreeCreationParameters(),
                websitePolicies ? std::make_optional(websitePolicies->dataForProcess(m_process)) : std::nullopt
            })
        ), 0
    );
}

void RemotePageProxy::processDidTerminate(WebProcessProxy& process, ProcessTerminationReason reason)
{
    RefPtr page = m_page.get();
    if (!page)
        return;
    if (RefPtr drawingArea = page->drawingArea())
        drawingArea->remotePageProcessDidTerminate(process.coreProcessIdentifier());
    if (RefPtr mainFrame = page->mainFrame())
        mainFrame->remoteProcessDidTerminate(process, WebFrameProxy::ClearFrameTreeSyncData::Yes);
    page->dispatchProcessDidTerminate(process, reason);
}

RemotePageProxy::~RemotePageProxy()
{
    ASSERT(m_disconnected);
}

void RemotePageProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageName() == Messages::WebPageProxy::IsPlayingMediaDidChange::name()) {
        IPC::handleMessage<Messages::WebPageProxy::IsPlayingMediaDidChange>(connection, decoder, this, &RemotePageProxy::isPlayingMediaDidChange);
        return;
    }
    if (decoder.messageName() == Messages::WebPageProxy::SetNetworkRequestsInProgress::name()) {
        IPC::handleMessage<Messages::WebPageProxy::SetNetworkRequestsInProgress>(connection, decoder, this, &RemotePageProxy::setNetworkRequestsInProgress);
        return;
    }

    if (RefPtr page = m_page.get()) {
        if (decoder.messageReceiverName() == Messages::WebBackForwardList::messageReceiverName())
            page->backForwardListMessageReceiver().didReceiveMessage(connection, decoder);
        else
            page->didReceiveMessage(connection, decoder);
    }
}

void RemotePageProxy::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder)
{
    if (RefPtr page = m_page.get()) {
        if (decoder.messageReceiverName() == Messages::WebBackForwardList::messageReceiverName())
            page->backForwardListMessageReceiver().didReceiveSyncMessage(connection, decoder, encoder);
        else
            page->didReceiveSyncMessage(connection, decoder, encoder);
    }
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
        UserMediaProcessManager::singleton().revokeSandboxExtensionsIfNeeded(m_process);
#endif
}

void RemotePageProxy::setNetworkRequestsInProgress(bool hasNetworkRequestsInProgress)
{
    m_hasNetworkRequestsInProgress = hasNetworkRequestsInProgress;

    RefPtr page = m_page.get();
    if (!page || page->isClosed())
        return;

    page->networkRequestsInProgressDidChange();
}

void RemotePageProxy::setDrawingArea(DrawingAreaProxy* drawingArea)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    RefPtr mainFrame = page->mainFrame();
    if (!mainFrame)
        return;

    if (!drawingArea) {
        m_drawingArea = nullptr;
        return;
    }

    m_drawingArea = RemotePageDrawingAreaProxy::create(*drawingArea, m_process);
    RefPtr websitePolicies = page->mainFrameWebsitePolicies();
    m_process->send(
        Messages::WebProcess::CreateWebPage(
            m_webPageID,
            page->creationParametersForRemotePage(m_process, *drawingArea, RemotePageParameters {
                URL(page->pageLoadState().url()),
                mainFrame->frameTreeCreationParameters(),
                websitePolicies ? std::make_optional(websitePolicies->dataForProcess(m_process)) : std::nullopt
            })
        ), 0
    );
}

void RemotePageProxy::setCurrentOrientation(WebCore::ScreenOrientationType orientation)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (RefPtr manager = page->screenOrientationManager())
        manager->setCurrentOrientation(orientation);
}

}
