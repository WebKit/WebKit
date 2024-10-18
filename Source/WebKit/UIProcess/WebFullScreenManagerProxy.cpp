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
    Ref webPageProxy = m_page.get();
    webPageProxy->protectedLegacyMainFrameProcess()->addMessageReceiver(Messages::WebFullScreenManagerProxy::messageReceiverName(), webPageProxy->webPageIDInMainFrameProcess(), *this);
}

WebFullScreenManagerProxy::~WebFullScreenManagerProxy()
{
    Ref webPageProxy = m_page.get();
    webPageProxy->protectedLegacyMainFrameProcess()->removeMessageReceiver(Messages::WebFullScreenManagerProxy::messageReceiverName(), webPageProxy->webPageIDInMainFrameProcess());
    m_client->closeFullScreenManager();
    callCloseCompletionHandlers();
}

Ref<WebPageProxy> WebFullScreenManagerProxy::protectedPage() const
{
    return m_page.get();
}

std::optional<SharedPreferencesForWebProcess> WebFullScreenManagerProxy::sharedPreferencesForWebProcess() const
{
    return protectedPage()->protectedLegacyMainFrameProcess()->sharedPreferencesForWebProcess();
}

void WebFullScreenManagerProxy::willEnterFullScreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_fullscreenState = FullscreenState::EnteringFullscreen;

    Ref webPageProxy = m_page.get();
    webPageProxy->fullscreenClient().willEnterFullscreen(webPageProxy.ptr());
    webPageProxy->protectedLegacyMainFrameProcess()->send(Messages::WebFullScreenManager::WillEnterFullScreen(mode), webPageProxy->webPageIDInMainFrameProcess());
}

void WebFullScreenManagerProxy::didEnterFullScreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_fullscreenState = FullscreenState::InFullscreen;
    Ref webPageProxy = m_page.get();
    webPageProxy->fullscreenClient().didEnterFullscreen(webPageProxy.ptr());
    webPageProxy->protectedLegacyMainFrameProcess()->send(Messages::WebFullScreenManager::DidEnterFullScreen(), webPageProxy->webPageIDInMainFrameProcess());

    if (webPageProxy->isControlledByAutomation()) {
        if (RefPtr automationSession = webPageProxy->protectedConfiguration()->processPool().automationSession())
            automationSession->didEnterFullScreenForPage(protectedPage());
    }
}

void WebFullScreenManagerProxy::willExitFullScreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_fullscreenState = FullscreenState::ExitingFullscreen;
    Ref webPageProxy = m_page.get();
    webPageProxy->fullscreenClient().willExitFullscreen(webPageProxy.ptr());
    webPageProxy->protectedLegacyMainFrameProcess()->send(Messages::WebFullScreenManager::WillExitFullScreen(), webPageProxy->webPageIDInMainFrameProcess());
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
    Ref webPageProxy = m_page.get();
    webPageProxy->fullscreenClient().didExitFullscreen(webPageProxy.ptr());
    webPageProxy->protectedLegacyMainFrameProcess()->send(Messages::WebFullScreenManager::DidExitFullScreen(), webPageProxy->webPageIDInMainFrameProcess());
    
    if (webPageProxy->isControlledByAutomation()) {
        if (RefPtr automationSession = webPageProxy->protectedConfiguration()->processPool().automationSession())
            automationSession->didExitFullScreenForPage(protectedPage());
    }
    callCloseCompletionHandlers();
}

void WebFullScreenManagerProxy::setAnimatingFullScreen(bool animating)
{
    Ref webPageProxy = m_page.get();
    webPageProxy->protectedLegacyMainFrameProcess()->send(Messages::WebFullScreenManager::SetAnimatingFullScreen(animating), webPageProxy->webPageIDInMainFrameProcess());
}

void WebFullScreenManagerProxy::requestRestoreFullScreen(CompletionHandler<void(bool)>&& completionHandler)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    Ref webPageProxy = m_page.get();
    webPageProxy->protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebFullScreenManager::RequestRestoreFullScreen(), WTFMove(completionHandler), webPageProxy->webPageIDInMainFrameProcess());
}

void WebFullScreenManagerProxy::requestExitFullScreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    Ref webPageProxy = m_page.get();
    webPageProxy->protectedLegacyMainFrameProcess()->send(Messages::WebFullScreenManager::RequestExitFullScreen(), webPageProxy->webPageIDInMainFrameProcess());
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
    Ref webPageProxy = m_page.get();
    webPageProxy->protectedLegacyMainFrameProcess()->send(Messages::WebFullScreenManager::SaveScrollPosition(), webPageProxy->webPageIDInMainFrameProcess());
}

void WebFullScreenManagerProxy::restoreScrollPosition()
{
    Ref webPageProxy = m_page.get();
    webPageProxy->protectedLegacyMainFrameProcess()->send(Messages::WebFullScreenManager::RestoreScrollPosition(), webPageProxy->webPageIDInMainFrameProcess());
}

void WebFullScreenManagerProxy::setFullscreenInsets(const WebCore::FloatBoxExtent& insets)
{
    Ref webPageProxy = m_page.get();
    webPageProxy->protectedLegacyMainFrameProcess()->send(Messages::WebFullScreenManager::SetFullscreenInsets(insets), webPageProxy->webPageIDInMainFrameProcess());
}

void WebFullScreenManagerProxy::setFullscreenAutoHideDuration(Seconds duration)
{
    Ref webPageProxy = m_page.get();
    webPageProxy->protectedLegacyMainFrameProcess()->send(Messages::WebFullScreenManager::SetFullscreenAutoHideDuration(duration), webPageProxy->webPageIDInMainFrameProcess());
}

void WebFullScreenManagerProxy::close()
{
    m_client->closeFullScreenManager();
}

bool WebFullScreenManagerProxy::isFullScreen()
{
    return m_client->isFullScreen();
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
    m_client->enterFullScreen(mediaDimensions);
#else
    UNUSED_PARAM(mediaDetails);
    m_client->enterFullScreen();
#endif
}

#if ENABLE(QUICKLOOK_FULLSCREEN)
void WebFullScreenManagerProxy::updateImageSource(FullScreenMediaDetails&& mediaDetails)
{
    if (mediaDetails.imageHandle) {
        if (auto sharedMemoryBuffer = SharedMemory::map(WTFMove(*mediaDetails.imageHandle), WebCore::SharedMemory::Protection::ReadOnly))
            m_imageBuffer = sharedMemoryBuffer->createSharedBuffer(sharedMemoryBuffer->size());
    }
    m_imageMIMEType = mediaDetails.mimeType;

    m_client->updateImageSource();
}
#endif // ENABLE(QUICKLOOK_FULLSCREEN)

void WebFullScreenManagerProxy::exitFullScreen()
{
#if ENABLE(QUICKLOOK_FULLSCREEN)
    m_imageBuffer = nullptr;
#endif
    m_client->exitFullScreen();
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
    protectedPage()->callAfterNextPresentationUpdate([weakThis = WeakPtr { *this }, initialFrame = initialFrame, finalFrame = finalFrame] {
        if (weakThis)
            weakThis->m_client->beganEnterFullScreen(initialFrame, finalFrame);
    });
}

void WebFullScreenManagerProxy::beganExitFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    m_client->beganExitFullScreen(initialFrame, finalFrame);
}

bool WebFullScreenManagerProxy::lockFullscreenOrientation(WebCore::ScreenOrientationType orientation)
{
    return m_client->lockFullscreenOrientation(orientation);
}

void WebFullScreenManagerProxy::unlockFullscreenOrientation()
{
    m_client->unlockFullscreenOrientation();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& WebFullScreenManagerProxy::logChannel() const
{
    return WebKit2LogFullscreen;
}
#endif

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
