/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WebFullScreenManagerProxy.h"

#if ENABLE(FULLSCREEN_API)

#include "APIFullscreenClient.h"
#include "APIPageConfiguration.h"
#include "MessageSenderInlines.h"
#include "WebAutomationSession.h"
#include "WebFullScreenManagerMessages.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <WebCore/IntRect.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/ScreenOrientationType.h>
#include <wtf/LoggerHelper.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebKit {
using namespace WebCore;

#if ENABLE(QUICKLOOK_FULLSCREEN)
static WorkQueue& sharedQuickLookFileQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("com.apple.WebKit.QuickLookFileQueue"_s, WorkQueue::QOS::UserInteractive));
    return queue.get();
}
#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebFullScreenManagerProxy);

WebFullScreenManagerProxy::WebFullScreenManagerProxy(WebPageProxy& page, WebFullScreenManagerProxyClient& client)
    : m_page(page)
    , m_client(client)
#if !RELEASE_LOG_DISABLED
    , m_logger(page.logger())
    , m_logIdentifier(page.logIdentifier())
#endif
{
    m_page.legacyMainFrameProcess().addMessageReceiver(Messages::WebFullScreenManagerProxy::messageReceiverName(), m_page.webPageIDInMainFrameProcess(), *this);
}

WebFullScreenManagerProxy::~WebFullScreenManagerProxy()
{
    m_page.legacyMainFrameProcess().removeMessageReceiver(Messages::WebFullScreenManagerProxy::messageReceiverName(), m_page.webPageIDInMainFrameProcess());
    m_client.closeFullScreenManager();
    callCloseCompletionHandlers();
}

const SharedPreferencesForWebProcess& WebFullScreenManagerProxy::sharedPreferencesForWebProcess() const
{
    return m_page.legacyMainFrameProcess().sharedPreferencesForWebProcess();
}

void WebFullScreenManagerProxy::willEnterFullScreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_fullscreenState = FullscreenState::EnteringFullscreen;
    m_page.fullscreenClient().willEnterFullscreen(&m_page);
    m_page.legacyMainFrameProcess().send(Messages::WebFullScreenManager::WillEnterFullScreen(mode), m_page.webPageIDInMainFrameProcess());
}

void WebFullScreenManagerProxy::didEnterFullScreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_fullscreenState = FullscreenState::InFullscreen;
    m_page.fullscreenClient().didEnterFullscreen(&m_page);
    m_page.legacyMainFrameProcess().send(Messages::WebFullScreenManager::DidEnterFullScreen(), m_page.webPageIDInMainFrameProcess());

    if (m_page.isControlledByAutomation()) {
        if (WebAutomationSession* automationSession = m_page.configuration().processPool().automationSession())
            automationSession->didEnterFullScreenForPage(m_page);
    }
}

void WebFullScreenManagerProxy::willExitFullScreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_fullscreenState = FullscreenState::ExitingFullscreen;
    m_page.fullscreenClient().willExitFullscreen(&m_page);
    m_page.legacyMainFrameProcess().send(Messages::WebFullScreenManager::WillExitFullScreen(), m_page.webPageIDInMainFrameProcess());
}

void WebFullScreenManagerProxy::callCloseCompletionHandlers()
{
    auto closeMediaCallbacks = WTFMove(m_closeCompletionHandlers);
    for (auto& callback : closeMediaCallbacks)
        callback();
}

void WebFullScreenManagerProxy::closeWithCallback(CompletionHandler<void()>&& completionHandler)
{
    m_closeCompletionHandlers.append(WTFMove(completionHandler));
    close();
}

void WebFullScreenManagerProxy::didExitFullScreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_fullscreenState = FullscreenState::NotInFullscreen;
    m_page.fullscreenClient().didExitFullscreen(&m_page);
    m_page.legacyMainFrameProcess().send(Messages::WebFullScreenManager::DidExitFullScreen(), m_page.webPageIDInMainFrameProcess());
    
    if (m_page.isControlledByAutomation()) {
        if (WebAutomationSession* automationSession = m_page.configuration().processPool().automationSession())
            automationSession->didExitFullScreenForPage(m_page);
    }
    callCloseCompletionHandlers();
}

void WebFullScreenManagerProxy::setAnimatingFullScreen(bool animating)
{
    m_page.legacyMainFrameProcess().send(Messages::WebFullScreenManager::SetAnimatingFullScreen(animating), m_page.webPageIDInMainFrameProcess());
}

void WebFullScreenManagerProxy::requestRestoreFullScreen(CompletionHandler<void(bool)>&& completionHandler)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_page.legacyMainFrameProcess().sendWithAsyncReply(Messages::WebFullScreenManager::RequestRestoreFullScreen(), WTFMove(completionHandler), m_page.webPageIDInMainFrameProcess());
}

void WebFullScreenManagerProxy::requestExitFullScreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_page.legacyMainFrameProcess().send(Messages::WebFullScreenManager::RequestExitFullScreen(), m_page.webPageIDInMainFrameProcess());
}

void WebFullScreenManagerProxy::supportsFullScreen(bool withKeyboard, CompletionHandler<void(bool)>&& completionHandler)
{
#if PLATFORM(IOS_FAMILY)
    completionHandler(!withKeyboard);
#else
    completionHandler(true);
#endif
}

void WebFullScreenManagerProxy::saveScrollPosition()
{
    m_page.legacyMainFrameProcess().send(Messages::WebFullScreenManager::SaveScrollPosition(), m_page.webPageIDInMainFrameProcess());
}

void WebFullScreenManagerProxy::restoreScrollPosition()
{
    m_page.legacyMainFrameProcess().send(Messages::WebFullScreenManager::RestoreScrollPosition(), m_page.webPageIDInMainFrameProcess());
}

void WebFullScreenManagerProxy::setFullscreenInsets(const WebCore::FloatBoxExtent& insets)
{
    m_page.legacyMainFrameProcess().send(Messages::WebFullScreenManager::SetFullscreenInsets(insets), m_page.webPageIDInMainFrameProcess());
}

void WebFullScreenManagerProxy::setFullscreenAutoHideDuration(Seconds duration)
{
    m_page.legacyMainFrameProcess().send(Messages::WebFullScreenManager::SetFullscreenAutoHideDuration(duration), m_page.webPageIDInMainFrameProcess());
}

void WebFullScreenManagerProxy::close()
{
    m_client.closeFullScreenManager();
}

bool WebFullScreenManagerProxy::isFullScreen()
{
    return m_client.isFullScreen();
}

bool WebFullScreenManagerProxy::blocksReturnToFullscreenFromPictureInPicture() const
{
    return m_blocksReturnToFullscreenFromPictureInPicture;
}

void WebFullScreenManagerProxy::enterFullScreen(bool blocksReturnToFullscreenFromPictureInPicture, FullScreenMediaDetails&& mediaDetails)
{
    m_blocksReturnToFullscreenFromPictureInPicture = blocksReturnToFullscreenFromPictureInPicture;
#if PLATFORM(IOS_FAMILY)

#if ENABLE(VIDEO_USES_ELEMENT_FULLSCREEN)
    m_isVideoElement = mediaDetails.type == FullScreenMediaDetails::Type::Video;
#endif
#if ENABLE(QUICKLOOK_FULLSCREEN)
    if (mediaDetails.imageHandle) {
        auto sharedMemoryBuffer = SharedMemory::map(WTFMove(*mediaDetails.imageHandle), WebCore::SharedMemory::Protection::ReadOnly);
        if (sharedMemoryBuffer)
            m_imageBuffer = sharedMemoryBuffer->createSharedBuffer(sharedMemoryBuffer->size());
    }
    m_imageMIMEType = mediaDetails.mimeType;
#endif // QUICKLOOK_FULLSCREEN

    auto mediaDimensions = mediaDetails.mediaDimensions;
    m_client.enterFullScreen(mediaDimensions);
#else
    UNUSED_PARAM(mediaDetails);
    m_client.enterFullScreen();
#endif
}

void WebFullScreenManagerProxy::exitFullScreen()
{
#if ENABLE(QUICKLOOK_FULLSCREEN)
    m_imageBuffer = nullptr;
#endif
    m_client.exitFullScreen();
}

#if ENABLE(QUICKLOOK_FULLSCREEN)
void WebFullScreenManagerProxy::prepareQuickLookImageURL(CompletionHandler<void(URL&&)>&& completionHandler) const
{
    if (!m_imageBuffer)
        return completionHandler(URL());

    sharedQuickLookFileQueue().dispatch([buffer = m_imageBuffer, mimeType = crossThreadCopy(m_imageMIMEType), completionHandler = WTFMove(completionHandler)]() mutable {
        auto suffix = makeString('.', WebCore::MIMETypeRegistry::preferredExtensionForMIMEType(mimeType));
        auto [filePath, fileHandle] = FileSystem::openTemporaryFile("QuickLook"_s, suffix);
        ASSERT(FileSystem::isHandleValid(fileHandle));

        size_t byteCount = FileSystem::writeToFile(fileHandle, buffer->span());
        ASSERT_UNUSED(byteCount, byteCount == buffer->size());
        FileSystem::closeFile(fileHandle);

        RunLoop::main().dispatch([filePath, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(URL::fileURLWithFileSystemPath(filePath));
        });
    });
}
#endif

void WebFullScreenManagerProxy::beganEnterFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    m_page.callAfterNextPresentationUpdate([weakThis = WeakPtr { *this }, initialFrame = initialFrame, finalFrame = finalFrame] {
        if (weakThis)
            weakThis->m_client.beganEnterFullScreen(initialFrame, finalFrame);
    });
}

void WebFullScreenManagerProxy::beganExitFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    m_client.beganExitFullScreen(initialFrame, finalFrame);
}

bool WebFullScreenManagerProxy::lockFullscreenOrientation(WebCore::ScreenOrientationType orientation)
{
    return m_client.lockFullscreenOrientation(orientation);
}

void WebFullScreenManagerProxy::unlockFullscreenOrientation()
{
    m_client.unlockFullscreenOrientation();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& WebFullScreenManagerProxy::logChannel() const
{
    return WebKit2LogFullscreen;
}
#endif

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
