/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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
#import "WebPageProxy.h"

#import "APIAttachment.h"
#import "APIPageConfiguration.h"
#import "APIUIClient.h"
#import "AppleMediaServicesUISPI.h"
#import "CocoaImage.h"
#import "Connection.h"
#import "DataDetectionResult.h"
#import "InsertTextOptions.h"
#import "LoadParameters.h"
#import "ModalContainerControlClassifier.h"
#import "NetworkConnectionIntegrityHelpers.h"
#import "PageClient.h"
#import "PlaybackSessionManagerProxy.h"
#import "QuickLookThumbnailLoader.h"
#import "RemoteLayerTreeTransaction.h"
#import "SafeBrowsingSPI.h"
#import "SafeBrowsingWarning.h"
#import "SharedBufferReference.h"
#import "SynapseSPI.h"
#import "VideoFullscreenManagerProxy.h"
#import "WKErrorInternal.h"
#import "WKWebView.h"
#import "WebContextMenuProxy.h"
#import "WebPage.h"
#import "WebPageMessages.h"
#import "WebPasteboardProxy.h"
#import "WebProcessMessages.h"
#import "WebProcessProxy.h"
#import "WebScreenOrientationManagerProxy.h"
#import "WebsiteDataStore.h"
#import <Foundation/NSURLRequest.h>
#import <WebCore/ApplePayAMSUIRequest.h>
#import <WebCore/DragItem.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/HighlightVisibility.h>
#import <WebCore/LocalCurrentGraphicsContext.h>
#import <WebCore/NetworkExtensionContentFilter.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/RunLoopObserver.h>
#import <WebCore/SearchPopupMenuCocoa.h>
#import <WebCore/TextAlternativeWithRange.h>
#import <WebCore/ValidationBubble.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/mac/QuarantineSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/SoftLinking.h>
#import <wtf/cf/TypeCastsCF.h>

#if ENABLE(MEDIA_USAGE)
#import "MediaUsageManagerCocoa.h"
#endif

#if ENABLE(APP_HIGHLIGHTS)
SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(Synapse)
SOFT_LINK_CLASS_OPTIONAL(Synapse, SYNotesActivationObserver)
#endif

#if USE(APPKIT)
#import <AppKit/NSImage.h>
#else
#import <UIKit/UIImage.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <WebCore/RenderThemeIOS.h>
#import "UIKitSPI.h"
#else
#import <WebCore/RenderThemeMac.h>
#endif

#if PLATFORM(IOS)
#import <pal/spi/cocoa/WebFilterEvaluatorSPI.h>

SOFT_LINK_PRIVATE_FRAMEWORK(WebContentAnalysis);
SOFT_LINK_CLASS(WebContentAnalysis, WebFilterEvaluator);
#endif

#if HAVE(SC_CONTENT_SHARING_SESSION)
#import <WebCore/ScreenCaptureKitSharingSessionManager.h>
#endif

#if ENABLE(APPLE_PAY_AMS_UI)
SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(AppleMediaServices)
SOFT_LINK_CLASS_OPTIONAL(AppleMediaServices, AMSEngagementRequest)

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(AppleMediaServicesUI)
SOFT_LINK_CLASS_OPTIONAL(AppleMediaServicesUI, AMSUIEngagementTask)
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, process().connection())
#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, process().connection(), completion)

#define WEBPAGEPROXY_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [pageProxyID=%llu, webPageID=%llu, PID=%i] WebPageProxy::" fmt, this, m_identifier.toUInt64(), m_webPageID.toUInt64(), m_process->processIdentifier(), ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

constexpr IntSize iconSize = IntSize(400, 400);

static bool exceedsRenderTreeSizeSizeThreshold(uint64_t thresholdSize, uint64_t committedSize)
{
    const double thesholdSizeFraction = 0.5; // Empirically-derived.
    return committedSize > thresholdSize * thesholdSizeFraction;
}

void WebPageProxy::didCommitLayerTree(const WebKit::RemoteLayerTreeTransaction& layerTreeTransaction)
{
    themeColorChanged(layerTreeTransaction.themeColor());
    pageExtendedBackgroundColorDidChange(layerTreeTransaction.pageExtendedBackgroundColor());
    sampledPageTopColorChanged(layerTreeTransaction.sampledPageTopColor());

    if (!m_hasUpdatedRenderingAfterDidCommitLoad) {
        if (layerTreeTransaction.transactionID() >= m_firstLayerTreeTransactionIdAfterDidCommitLoad) {
            m_hasUpdatedRenderingAfterDidCommitLoad = true;
            stopMakingViewBlankDueToLackOfRenderingUpdateIfNecessary();
            m_lastVisibleContentRectUpdate = VisibleContentRectUpdateInfo();
        }
    }

    pageClient().didCommitLayerTree(layerTreeTransaction);

    // FIXME: Remove this special mechanism and fold it into the transaction's layout milestones.
    if (m_observedLayoutMilestones.contains(WebCore::ReachedSessionRestorationRenderTreeSizeThreshold) && !m_hitRenderTreeSizeThreshold
        && exceedsRenderTreeSizeSizeThreshold(m_sessionRestorationRenderTreeSize, layerTreeTransaction.renderTreeSize())) {
        m_hitRenderTreeSizeThreshold = true;
        didReachLayoutMilestone(WebCore::ReachedSessionRestorationRenderTreeSizeThreshold);
    }
}

void WebPageProxy::layerTreeCommitComplete()
{
    pageClient().layerTreeCommitComplete();
}

#if ENABLE(DATA_DETECTION)

void WebPageProxy::setDataDetectionResult(const DataDetectionResult& dataDetectionResult)
{
    m_dataDetectionResults = dataDetectionResult.results;
}

void WebPageProxy::handleClickForDataDetectionResult(const DataDetectorElementInfo& info, const IntPoint& clickLocation)
{
    pageClient().handleClickForDataDetectionResult(info, clickLocation);
}

#endif

void WebPageProxy::saveRecentSearches(const String& name, const Vector<WebCore::RecentSearch>& searchItems)
{
    MESSAGE_CHECK(!name.isNull());

    WebCore::saveRecentSearches(name, searchItems);
}

void WebPageProxy::loadRecentSearches(const String& name, CompletionHandler<void(Vector<WebCore::RecentSearch>&&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!name.isNull(), completionHandler({ }));

    completionHandler(WebCore::loadRecentSearches(name));
}

void WebPageProxy::grantAccessToCurrentPasteboardData(const String& pasteboardName)
{
    if (!hasRunningProcess())
        return;

    WebPasteboardProxy::singleton().grantAccessToCurrentData(m_process.get(), pasteboardName);
}

void WebPageProxy::beginSafeBrowsingCheck(const URL& url, bool forMainFrameNavigation, WebFramePolicyListenerProxy& listener)
{
#if HAVE(SAFE_BROWSING)
    SSBLookupContext *context = [SSBLookupContext sharedLookupContext];
    if (!context)
        return listener.didReceiveSafeBrowsingResults({ });
    [context lookUpURL:url completionHandler:makeBlockPtr([listener = Ref { listener }, forMainFrameNavigation, url = url] (SSBLookupResult *result, NSError *error) mutable {
        RunLoop::main().dispatch([listener = WTFMove(listener), result = retainPtr(result), error = retainPtr(error), forMainFrameNavigation, url = WTFMove(url)] {
            if (error) {
                listener->didReceiveSafeBrowsingResults({ });
                return;
            }

            for (SSBServiceLookupResult *lookupResult in [result serviceLookupResults]) {
                if (lookupResult.isPhishing || lookupResult.isMalware || lookupResult.isUnwantedSoftware) {
                    listener->didReceiveSafeBrowsingResults(SafeBrowsingWarning::create(url, forMainFrameNavigation, lookupResult));
                    return;
                }
            }
            listener->didReceiveSafeBrowsingResults({ });
        });
    }).get()];
#else
    listener.didReceiveSafeBrowsingResults({ });
#endif
}

#if ENABLE(CONTENT_FILTERING)
void WebPageProxy::contentFilterDidBlockLoadForFrame(const WebCore::ContentFilterUnblockHandler& unblockHandler, FrameIdentifier frameID)
{
    contentFilterDidBlockLoadForFrameShared(m_process.copyRef(), unblockHandler, frameID);
}

void WebPageProxy::contentFilterDidBlockLoadForFrameShared(Ref<WebProcessProxy>&& process, const WebCore::ContentFilterUnblockHandler& unblockHandler, FrameIdentifier frameID)
{
    if (RefPtr frame = WebFrameProxy::webFrame(frameID))
        frame->contentFilterDidBlockLoad(unblockHandler);
}
#endif

void WebPageProxy::addPlatformLoadParameters(WebProcessProxy& process, LoadParameters& loadParameters)
{
    loadParameters.dataDetectionContext = m_uiClient->dataDetectionContext();

#if !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    loadParameters.networkExtensionSandboxExtensionHandles = createNetworkExtensionsSandboxExtensions(process);
#if PLATFORM(IOS)
    auto auditToken = process.auditToken();
    if (!process.hasManagedSessionSandboxAccess() && [getWebFilterEvaluatorClass() isManagedSession]) {
        if (auto handle = SandboxExtension::createHandleForMachLookup("com.apple.uikit.viewservice.com.apple.WebContentFilter.remoteUI"_s, auditToken, SandboxExtension::MachBootstrapOptions::EnableMachBootstrap))
            loadParameters.contentFilterExtensionHandle = WTFMove(*handle);

        if (auto handle = SandboxExtension::createHandleForMachLookup("com.apple.frontboard.systemappservices"_s, auditToken, SandboxExtension::MachBootstrapOptions::EnableMachBootstrap))
            loadParameters.frontboardServiceExtensionHandle = WTFMove(*handle);

        process.markHasManagedSessionSandboxAccess();
    }
#endif // PLATFORM(IOS)
#endif // !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
}

void WebPageProxy::createSandboxExtensionsIfNeeded(const Vector<String>& files, SandboxExtension::Handle& fileReadHandle, Vector<SandboxExtension::Handle>& fileUploadHandles)
{
    if (!files.size())
        return;

    if (files.size() == 1) {
        BOOL isDirectory;
        if ([[NSFileManager defaultManager] fileExistsAtPath:files[0] isDirectory:&isDirectory] && !isDirectory) {
            ASSERT(process().connection() && process().connection()->getAuditToken());
            if (process().connection() && process().connection()->getAuditToken()) {
                if (auto handle = SandboxExtension::createHandleForReadByAuditToken("/"_s, *(process().connection()->getAuditToken())))
                    fileReadHandle = WTFMove(*handle);
            } else if (auto handle = SandboxExtension::createHandle("/"_s, SandboxExtension::Type::ReadOnly))
                fileReadHandle = WTFMove(*handle);
            willAcquireUniversalFileReadSandboxExtension(m_process);
        }
    }

    for (auto& file : files) {
        if (![[NSFileManager defaultManager] fileExistsAtPath:file])
            continue;
        if (auto handle = SandboxExtension::createHandle(file, SandboxExtension::Type::ReadOnly))
            fileUploadHandles.append(WTFMove(*handle));
    }
}

bool WebPageProxy::scrollingUpdatesDisabledForTesting()
{
    return pageClient().scrollingUpdatesDisabledForTesting();
}

#if ENABLE(DRAG_SUPPORT)

void WebPageProxy::startDrag(const DragItem& dragItem, const ShareableBitmapHandle& dragImageHandle)
{
    pageClient().startDrag(dragItem, dragImageHandle);
}

// FIXME: Move these functions to WebPageProxyIOS.mm.
#if PLATFORM(IOS_FAMILY)

void WebPageProxy::setPromisedDataForImage(const String&, const SharedMemory::Handle&, const String&, const String&, const String&, const String&, const String&, const SharedMemory::Handle&, const String&)
{
    notImplemented();
}

void WebPageProxy::setDragCaretRect(const IntRect& dragCaretRect)
{
    if (m_currentDragCaretRect == dragCaretRect)
        return;

    auto previousRect = m_currentDragCaretRect;
    m_currentDragCaretRect = dragCaretRect;
    pageClient().didChangeDragCaretRect(previousRect, dragCaretRect);
}

#endif // PLATFORM(IOS_FAMILY)

#endif // ENABLE(DRAG_SUPPORT)

#if ENABLE(ATTACHMENT_ELEMENT)

void WebPageProxy::platformRegisterAttachment(Ref<API::Attachment>&& attachment, const String& preferredFileName, const IPC::SharedBufferReference& bufferCopy)
{
    if (bufferCopy.isEmpty())
        return;

    auto fileWrapper = adoptNS([pageClient().allocFileWrapperInstance() initRegularFileWithContents:bufferCopy.unsafeBuffer()->createNSData().get()]);
    [fileWrapper setPreferredFilename:preferredFileName];
    attachment->setFileWrapper(fileWrapper.get());
}

void WebPageProxy::platformRegisterAttachment(Ref<API::Attachment>&& attachment, const String& filePath)
{
    if (!filePath)
        return;

    auto fileWrapper = adoptNS([pageClient().allocFileWrapperInstance() initWithURL:[NSURL fileURLWithPath:filePath] options:0 error:nil]);
    attachment->setFileWrapper(fileWrapper.get());
}

void WebPageProxy::platformCloneAttachment(Ref<API::Attachment>&& fromAttachment, Ref<API::Attachment>&& toAttachment)
{
    fromAttachment->doWithFileWrapper([&](NSFileWrapper *fileWrapper) {
        toAttachment->setFileWrapper(fileWrapper);
    });
}

static RefPtr<WebKit::ShareableBitmap> convertPlatformImageToBitmap(CocoaImage *image, const WebCore::FloatSize& fittingSize)
{
    FloatSize originalThumbnailSize([image size]);
    auto resultRect = roundedIntRect(largestRectWithAspectRatioInsideRect(originalThumbnailSize.aspectRatio(), { { }, fittingSize }));
    resultRect.setLocation({ });

    WebKit::ShareableBitmapConfiguration bitmapConfiguration;
    auto bitmap = WebKit::ShareableBitmap::create(resultRect.size(), bitmapConfiguration);
    if (!bitmap)
        return nullptr;

    auto graphicsContext = bitmap->createGraphicsContext();
    if (!graphicsContext)
        return nullptr;

    LocalCurrentGraphicsContext savedContext(*graphicsContext);
    [image drawInRect:resultRect];

    return bitmap;
}

RefPtr<WebKit::ShareableBitmap> WebPageProxy::iconForAttachment(const String& fileName, const String& contentType, const String& title, FloatSize& size)
{
#if PLATFORM(IOS_FAMILY)
    auto imageAndSize = RenderThemeIOS::iconForAttachment(fileName, contentType, title);
    auto image = imageAndSize.icon;
    size = imageAndSize.size;
#else
    auto image = RenderThemeMac::iconForAttachment(fileName, contentType, title);
#endif
    return convertPlatformImageToBitmap(image.get(), iconSize);
}

#endif // ENABLE(ATTACHMENT_ELEMENT)
    
void WebPageProxy::performDictionaryLookupAtLocation(const WebCore::FloatPoint& point)
{
    if (!hasRunningProcess())
        return;
    
    send(Messages::WebPage::PerformDictionaryLookupAtLocation(point));
}

void WebPageProxy::performDictionaryLookupOfCurrentSelection()
{
    if (!hasRunningProcess())
        return;
    
    send(Messages::WebPage::PerformDictionaryLookupOfCurrentSelection());
}

void WebPageProxy::insertDictatedTextAsync(const String& text, const EditingRange& replacementRange, const Vector<TextAlternativeWithRange>& dictationAlternativesWithRange, InsertTextOptions&& options)
{
    if (!hasRunningProcess())
        return;

    Vector<DictationAlternative> dictationAlternatives;
    for (const auto& alternativeWithRange : dictationAlternativesWithRange) {
        if (auto context = pageClient().addDictationAlternatives(alternativeWithRange.alternatives.get()))
            dictationAlternatives.append({ alternativeWithRange.range, context });
    }

    if (dictationAlternatives.isEmpty()) {
        insertTextAsync(text, replacementRange, WTFMove(options));
        return;
    }

    send(Messages::WebPage::InsertDictatedTextAsync { text, replacementRange, dictationAlternatives, WTFMove(options) });
}

void WebPageProxy::addDictationAlternative(TextAlternativeWithRange&& alternative)
{
    if (!hasRunningProcess())
        return;

    auto nsAlternatives = alternative.alternatives.get();
    auto context = pageClient().addDictationAlternatives(nsAlternatives);
    sendWithAsyncReply(Messages::WebPage::AddDictationAlternative { nsAlternatives.primaryString, context }, [context, weakThis = WeakPtr { *this }](bool success) {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && !success)
            protectedThis->removeDictationAlternatives(context);
    });
}

void WebPageProxy::dictationAlternativesAtSelection(CompletionHandler<void(Vector<DictationContext>&&)>&& completion)
{
    if (!hasRunningProcess()) {
        completion({ });
        return;
    }

    sendWithAsyncReply(Messages::WebPage::DictationAlternativesAtSelection(), WTFMove(completion));
}

void WebPageProxy::clearDictationAlternatives(Vector<DictationContext>&& alternativesToClear)
{
    if (!hasRunningProcess() || alternativesToClear.isEmpty())
        return;

    send(Messages::WebPage::ClearDictationAlternatives(WTFMove(alternativesToClear)));
}

#if USE(DICTATION_ALTERNATIVES)

NSTextAlternatives *WebPageProxy::platformDictationAlternatives(WebCore::DictationContext dictationContext)
{
    return pageClient().platformDictationAlternatives(dictationContext);
}

#endif

ResourceError WebPageProxy::errorForUnpermittedAppBoundDomainNavigation(const URL& url)
{
    return { WKErrorDomain, WKErrorNavigationAppBoundDomain, url, localizedDescriptionForErrorCode(WKErrorNavigationAppBoundDomain) };
}
    
#if ENABLE(APPLE_PAY)

IPC::Connection* WebPageProxy::paymentCoordinatorConnection(const WebPaymentCoordinatorProxy&)
{
    return messageSenderConnection();
}

const String& WebPageProxy::paymentCoordinatorBoundInterfaceIdentifier(const WebPaymentCoordinatorProxy&)
{
    return websiteDataStore().configuration().boundInterfaceIdentifier();
}

const String& WebPageProxy::paymentCoordinatorSourceApplicationBundleIdentifier(const WebPaymentCoordinatorProxy&)
{
    return websiteDataStore().configuration().sourceApplicationBundleIdentifier();
}

const String& WebPageProxy::paymentCoordinatorSourceApplicationSecondaryIdentifier(const WebPaymentCoordinatorProxy&)
{
    return websiteDataStore().configuration().sourceApplicationSecondaryIdentifier();
}

void WebPageProxy::paymentCoordinatorAddMessageReceiver(WebPaymentCoordinatorProxy&, IPC::ReceiverName receiverName, IPC::MessageReceiver& messageReceiver)
{
    process().addMessageReceiver(receiverName, m_webPageID, messageReceiver);
}

void WebPageProxy::paymentCoordinatorRemoveMessageReceiver(WebPaymentCoordinatorProxy&, IPC::ReceiverName receiverName)
{
    process().removeMessageReceiver(receiverName, m_webPageID);
}

#endif

#if ENABLE(SPEECH_SYNTHESIS)
void WebPageProxy::didStartSpeaking(WebCore::PlatformSpeechSynthesisUtterance&)
{
    if (speechSynthesisData().speakingStartedCompletionHandler)
        speechSynthesisData().speakingStartedCompletionHandler();
}

void WebPageProxy::didFinishSpeaking(WebCore::PlatformSpeechSynthesisUtterance&)
{
    if (speechSynthesisData().speakingFinishedCompletionHandler)
        speechSynthesisData().speakingFinishedCompletionHandler();
}

void WebPageProxy::didPauseSpeaking(WebCore::PlatformSpeechSynthesisUtterance&)
{
    if (speechSynthesisData().speakingPausedCompletionHandler)
        speechSynthesisData().speakingPausedCompletionHandler();
}

void WebPageProxy::didResumeSpeaking(WebCore::PlatformSpeechSynthesisUtterance&)
{
    if (speechSynthesisData().speakingResumedCompletionHandler)
        speechSynthesisData().speakingResumedCompletionHandler();
}

void WebPageProxy::speakingErrorOccurred(WebCore::PlatformSpeechSynthesisUtterance&)
{
    send(Messages::WebPage::SpeakingErrorOccurred());
}

void WebPageProxy::boundaryEventOccurred(WebCore::PlatformSpeechSynthesisUtterance&, WebCore::SpeechBoundary speechBoundary, unsigned charIndex, unsigned charLength)
{
    send(Messages::WebPage::BoundaryEventOccurred(speechBoundary == WebCore::SpeechBoundary::SpeechWordBoundary, charIndex, charLength));
}

void WebPageProxy::voicesDidChange()
{
    send(Messages::WebPage::VoicesDidChange());
}
#endif // ENABLE(SPEECH_SYNTHESIS)

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
void WebPageProxy::didCreateContextInWebProcessForVisibilityPropagation(LayerHostingContextID contextID)
{
    m_contextIDForVisibilityPropagationInWebProcess = contextID;
    pageClient().didCreateContextInWebProcessForVisibilityPropagation(contextID);
}

#if ENABLE(GPU_PROCESS)
void WebPageProxy::didCreateContextInGPUProcessForVisibilityPropagation(LayerHostingContextID contextID)
{
    m_contextIDForVisibilityPropagationInGPUProcess = contextID;
    pageClient().didCreateContextInGPUProcessForVisibilityPropagation(contextID);
}
#endif // ENABLE(GPU_PROCESS)
#endif // HAVE(VISIBILITY_PROPAGATION_VIEW)

#if ENABLE(MEDIA_USAGE)
MediaUsageManager& WebPageProxy::mediaUsageManager()
{
    if (!m_mediaUsageManager)
        m_mediaUsageManager = MediaUsageManager::create();

    return *m_mediaUsageManager;
}

void WebPageProxy::addMediaUsageManagerSession(WebCore::MediaSessionIdentifier identifier, const String& bundleIdentifier, const URL& pageURL)
{
    mediaUsageManager().addMediaSession(identifier, bundleIdentifier, pageURL);
}

void WebPageProxy::updateMediaUsageManagerSessionState(WebCore::MediaSessionIdentifier identifier, const WebCore::MediaUsageInfo& info)
{
    mediaUsageManager().updateMediaUsage(identifier, info);
}

void WebPageProxy::removeMediaUsageManagerSession(WebCore::MediaSessionIdentifier identifier)
{
    mediaUsageManager().removeMediaSession(identifier);
}
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)

void WebPageProxy::didChangePlaybackRate(PlaybackSessionContextIdentifier identifier)
{
    if (m_currentFullscreenVideoSessionIdentifier == identifier)
        updateFullscreenVideoTextRecognition();
}

void WebPageProxy::didChangeCurrentTime(PlaybackSessionContextIdentifier identifier)
{
    if (m_currentFullscreenVideoSessionIdentifier == identifier)
        updateFullscreenVideoTextRecognition();
}

void WebPageProxy::updateFullscreenVideoTextRecognition()
{
    if (!pageClient().isTextRecognitionInFullscreenVideoEnabled())
        return;

    if (m_currentFullscreenVideoSessionIdentifier && m_playbackSessionManager && m_playbackSessionManager->isPaused(*m_currentFullscreenVideoSessionIdentifier)) {
        m_fullscreenVideoTextRecognitionTimer.startOneShot(250_ms);
        return;
    }

    m_fullscreenVideoTextRecognitionTimer.stop();

    if (!m_currentFullscreenVideoSessionIdentifier)
        return;

#if PLATFORM(IOS_FAMILY)
    if (RetainPtr controller = m_videoFullscreenManager->playerViewController(*m_currentFullscreenVideoSessionIdentifier))
        pageClient().cancelTextRecognitionForFullscreenVideo(controller.get());
#endif
}

void WebPageProxy::fullscreenVideoTextRecognitionTimerFired()
{
    if (!m_currentFullscreenVideoSessionIdentifier || !m_videoFullscreenManager)
        return;

    auto identifier = *m_currentFullscreenVideoSessionIdentifier;
    m_videoFullscreenManager->requestBitmapImageForCurrentTime(identifier, [identifier, weakThis = WeakPtr { *this }](auto& imageHandle) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || protectedThis->m_currentFullscreenVideoSessionIdentifier != identifier)
            return;

        auto fullscreenManager = protectedThis->m_videoFullscreenManager;
        if (!fullscreenManager)
            return;

#if PLATFORM(IOS_FAMILY)
        if (RetainPtr controller = fullscreenManager->playerViewController(identifier))
            protectedThis->pageClient().beginTextRecognitionForFullscreenVideo(imageHandle, controller.get());
#endif
    });
}

#endif // ENABLE(VIDEO_PRESENTATION_MODE)

#if HAVE(QUICKLOOK_THUMBNAILING)

void WebPageProxy::requestThumbnail(WKQLThumbnailLoadOperation *operation)
{
    [operation setCompletionBlock:^{
        RunLoop::main().dispatch([this, operation = retainPtr(operation)] {
            auto identifier = [operation identifier];
            auto convertedImage = convertPlatformImageToBitmap([operation thumbnail], iconSize);
            if (!convertedImage)
                return;
            this->updateAttachmentThumbnail(identifier, convertedImage);
        });
    }];

    [[WKQLThumbnailQueueManager sharedInstance].queue addOperation:operation];
}

void WebPageProxy::requestThumbnail(const API::Attachment& attachment, const String& identifier)
{
    requestThumbnail(adoptNS([[WKQLThumbnailLoadOperation alloc] initWithAttachment:attachment identifier:identifier]).get());
}

void WebPageProxy::requestThumbnailWithPath(const String& filePath, const String& identifier)
{
    requestThumbnail(adoptNS([[WKQLThumbnailLoadOperation alloc] initWithURL:filePath identifier:identifier]).get());
}

#endif // HAVE(QUICKLOOK_THUMBNAILING)

#if ENABLE(ATTACHMENT_ELEMENT) && PLATFORM(MAC)

bool WebPageProxy::updateIconForDirectory(NSFileWrapper *fileWrapper, const String& identifier)
{
    auto image = [fileWrapper icon];
    if (!image)
        return false;

    auto convertedImage = convertPlatformImageToBitmap(image, iconSize);
    if (!convertedImage)
        return false;

    auto handle = convertedImage->createHandle();
    if (!handle)
        return false;
    send(Messages::WebPage::UpdateAttachmentIcon(identifier, *handle, iconSize));
    return true;
}

#endif

void WebPageProxy::scheduleActivityStateUpdate()
{
    bool hasScheduledObserver = m_activityStateChangeDispatcher->isScheduled();
    bool hasActiveCATransaction = [CATransaction currentState];

    if (hasScheduledObserver && hasActiveCATransaction) {
        ASSERT(m_hasScheduledActivityStateUpdate);
        m_hasScheduledActivityStateUpdate = false;
        m_activityStateChangeDispatcher->invalidate();
    }

    if (m_hasScheduledActivityStateUpdate)
        return;
    m_hasScheduledActivityStateUpdate = true;

    // If there is an active transaction, we need to dispatch the update after the transaction is committed,
    // to avoid flash caused by web process setting root layer too early.
    // If there is no active transaction, likely there is no root layer change or change is committed,
    // then schedule dispatch on runloop observer to collect changes in the same runloop cycle before dispatching.
    if (hasActiveCATransaction) {
        [CATransaction addCommitHandler:[weakThis = WeakPtr { *this }] {
            // We can't call dispatchActivityStateChange directly underneath this commit handler, because it has side-effects
            // that may result in other frameworks trying to install commit handlers for the same phase, which is not allowed.
            // So, dispatch_async here; we only care that the activity state change doesn't apply until after the active commit is complete.
            WorkQueue::main().dispatch([weakThis] {
                RefPtr protectedThis { weakThis.get() };
                if (!protectedThis)
                    return;

                protectedThis->dispatchActivityStateChange();
            });
        } forPhase:kCATransactionPhasePostCommit];
        return;
    }

    m_activityStateChangeDispatcher->schedule();
}

void WebPageProxy::addActivityStateUpdateCompletionHandler(CompletionHandler<void()>&& completionHandler)
{
    if (!m_hasScheduledActivityStateUpdate) {
        completionHandler();
        return;
    }

    m_activityStateUpdateCallbacks.append(WTFMove(completionHandler));
}

#if ENABLE(APP_HIGHLIGHTS)
void WebPageProxy::createAppHighlightInSelectedRange(WebCore::CreateNewGroupForHighlight createNewGroup, WebCore::HighlightRequestOriginatedInApp requestOriginatedInApp)
{
    if (!hasRunningProcess())
        return;

    setUpHighlightsObserver();

    send(Messages::WebPage::CreateAppHighlightInSelectedRange(createNewGroup, requestOriginatedInApp));
}

void WebPageProxy::restoreAppHighlightsAndScrollToIndex(const Vector<Ref<SharedMemory>>& highlights, const std::optional<unsigned> index)
{
    if (!hasRunningProcess())
        return;

    auto memoryHandles = WTF::compactMap(highlights, [](auto& highlight) {
        return highlight->createHandle(SharedMemory::Protection::ReadOnly);
    });
    
    setUpHighlightsObserver();

    send(Messages::WebPage::RestoreAppHighlightsAndScrollToIndex(WTFMove(memoryHandles), index));
}

void WebPageProxy::setAppHighlightsVisibility(WebCore::HighlightVisibility appHighlightsVisibility)
{
    RELEASE_ASSERT(isMainRunLoop());
    
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetAppHighlightsVisibility(appHighlightsVisibility));
}

bool WebPageProxy::appHighlightsVisibility()
{
    return [m_appHighlightsObserver isVisible];
}

CGRect WebPageProxy::appHighlightsOverlayRect()
{
    if (!m_appHighlightsObserver)
        return CGRectNull;
    return [m_appHighlightsObserver visibleFrame];
}

void WebPageProxy::setUpHighlightsObserver()
{
    if (m_appHighlightsObserver)
        return;

    WeakPtr weakThis { *this };
    auto updateAppHighlightsVisibility = ^(BOOL isVisible) {
        ensureOnMainRunLoop([weakThis, isVisible] {
            if (!weakThis)
                return;
            weakThis->setAppHighlightsVisibility(isVisible ? WebCore::HighlightVisibility::Visible : WebCore::HighlightVisibility::Hidden);
        });
    };
    
    m_appHighlightsObserver = adoptNS([allocSYNotesActivationObserverInstance() initWithHandler:updateAppHighlightsVisibility]);
}

#endif

#if ENABLE(APPLE_PAY_AMS_UI)

void WebPageProxy::startApplePayAMSUISession(URL&& originatingURL, ApplePayAMSUIRequest&& request, CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    if (!AppleMediaServicesUILibrary()) {
        completionHandler(std::nullopt);
        return;
    }

    PlatformViewController *presentingViewController = uiClient().presentingViewController();
    if (!presentingViewController) {
        completionHandler(std::nullopt);
        return;
    }

    auto amsRequest = adoptNS([allocAMSEngagementRequestInstance() initWithRequestDictionary:dynamic_objc_cast<NSDictionary>([NSJSONSerialization JSONObjectWithData:[WTFMove(request.engagementRequest) dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil])]);
    [amsRequest setOriginatingURL:WTFMove(originatingURL)];

    auto amsBag = retainPtr([getAMSUIEngagementTaskClass() createBagForSubProfile]);

    m_applePayAMSUISession = adoptNS([allocAMSUIEngagementTaskInstance() initWithRequest:amsRequest.get() bag:amsBag.get() presentingViewController:presentingViewController]);
    [m_applePayAMSUISession setRemotePresentation:YES];

    auto amsResult = retainPtr([m_applePayAMSUISession presentEngagement]);
    [amsResult addFinishBlock:makeBlockPtr([completionHandler = WTFMove(completionHandler)] (AMSEngagementResult *result, NSError *error) mutable {
        if (error) {
            completionHandler(std::nullopt);
            return;
        }

        completionHandler(result);
    }).get()];
}

void WebPageProxy::abortApplePayAMSUISession()
{
    [std::exchange(m_applePayAMSUISession, nullptr) cancel];
}

#endif // ENABLE(APPLE_PAY_AMS_UI)

#if !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
Vector<SandboxExtension::Handle> WebPageProxy::createNetworkExtensionsSandboxExtensions(WebProcessProxy& process)
{
#if ENABLE(CONTENT_FILTERING)
    if (!process.hasNetworkExtensionSandboxAccess() && NetworkExtensionContentFilter::isRequired()) {
        process.markHasNetworkExtensionSandboxAccess();
        constexpr ASCIILiteral neHelperService { "com.apple.nehelper"_s };
        constexpr ASCIILiteral neSessionManagerService { "com.apple.nesessionmanager.content-filter"_s };
        auto auditToken = process.hasConnection() ? process.connection()->getAuditToken();
        return SandboxExtension::createHandlesForMachLookup({ neHelperService, neSessionManagerService }, auditToken, );
    }
#endif
    return { };
}
#endif

#if ENABLE(CONTEXT_MENUS)
#if HAVE(TRANSLATION_UI_SERVICES)

bool WebPageProxy::canHandleContextMenuTranslation() const
{
    return pageClient().canHandleContextMenuTranslation();
}

void WebPageProxy::handleContextMenuTranslation(const TranslationContextMenuInfo& info)
{
    return pageClient().handleContextMenuTranslation(info);
}

#endif // HAVE(TRANSLATION_UI_SERVICES)
#endif // ENABLE(CONTEXT_MENUS)

void WebPageProxy::requestActiveNowPlayingSessionInfo(CompletionHandler<void(bool, bool, const String&, double, double, uint64_t)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::RequestActiveNowPlayingSessionInfo(), WTFMove(callback));
}

void WebPageProxy::setLastNavigationWasAppInitiated(ResourceRequest& request)
{
#if ENABLE(APP_PRIVACY_REPORT)
    auto isAppInitiated = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody).attribution == NSURLRequestAttributionDeveloper;
    if (m_configuration->appInitiatedOverrideValueForTesting() != AttributionOverrideTesting::NoOverride)
        isAppInitiated = m_configuration->appInitiatedOverrideValueForTesting() == AttributionOverrideTesting::AppInitiated;

    request.setIsAppInitiated(isAppInitiated);
    m_lastNavigationWasAppInitiated = isAppInitiated;
#endif
}

void WebPageProxy::lastNavigationWasAppInitiated(CompletionHandler<void(bool)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPage::LastNavigationWasAppInitiated(), WTFMove(completionHandler));
}

void WebPageProxy::grantAccessToAssetServices()
{
    auto handles = SandboxExtension::createHandlesForMachLookup({ "com.apple.mobileassetd.v2"_s }, process().auditToken(), SandboxExtension::MachBootstrapOptions::EnableMachBootstrap);
    process().send(Messages::WebProcess::GrantAccessToAssetServices(handles), 0);
}

void WebPageProxy::revokeAccessToAssetServices()
{
    process().send(Messages::WebProcess::RevokeAccessToAssetServices(), 0);
}

void WebPageProxy::disableURLSchemeCheckInDataDetectors() const
{
    process().send(Messages::WebProcess::DisableURLSchemeCheckInDataDetectors(), 0);
}

void WebPageProxy::switchFromStaticFontRegistryToUserFontRegistry()
{
    process().send(Messages::WebProcess::SwitchFromStaticFontRegistryToUserFontRegistry(process().fontdMachExtensionHandles(SandboxExtension::MachBootstrapOptions::EnableMachBootstrap)), 0);
}

NSDictionary *WebPageProxy::contentsOfUserInterfaceItem(NSString *userInterfaceItem)
{
#if ENABLE(CONTEXT_MENUS)
    if (m_activeContextMenu && [userInterfaceItem isEqualToString:@"mediaControlsContextMenu"])
        return @{ userInterfaceItem: m_activeContextMenu->platformData() };
#endif // ENABLE(CONTEXT_MENUS)

    return nil;
}

#if PLATFORM(MAC)
bool WebPageProxy::isQuarantinedAndNotUserApproved(const String& fileURLString)
{
    if (!fileURLString.endsWithIgnoringASCIICase(".webarchive"_s))
        return false;

    NSURL *fileURL = [NSURL URLWithString:fileURLString];
    qtn_file_t qf = qtn_file_alloc();

    int quarantineError = qtn_file_init_with_path(qf, fileURL.path.fileSystemRepresentation);

    if (quarantineError == ENOENT || quarantineError == QTN_NOT_QUARANTINED)
        return false;

    if (quarantineError) {
        // If we fail to check the quarantine status, assume the file is quarantined and not user approved to be safe.
        WEBPAGEPROXY_RELEASE_LOG(Loading, "isQuarantinedAndNotUserApproved: failed to initialize quarantine file with path.");
        qtn_file_free(qf);
        return true;
    }

    uint32_t fileflags = qtn_file_get_flags(qf);
    qtn_file_free(qf);

    if (fileflags & QTN_FLAG_USER_APPROVED)
        return false;

    return true;
}
#endif

void WebPageProxy::classifyModalContainerControls(Vector<String>&& texts, CompletionHandler<void(Vector<ModalContainerControlType>&&)>&& completion)
{
    ModalContainerControlClassifier::sharedClassifier().classify(WTFMove(texts), WTFMove(completion));
}

void WebPageProxy::replaceSelectionWithPasteboardData(const Vector<String>& types, const IPC::DataReference& data)
{
    send(Messages::WebPage::ReplaceSelectionWithPasteboardData(types, data));
}

void WebPageProxy::setCocoaView(WKWebView *view)
{
    m_cocoaView = view;
#if PLATFORM(IOS_FAMILY)
    if (m_screenOrientationManager)
        m_screenOrientationManager->setWindow(view.window);
#endif
}

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

void WebPageProxy::replaceImageForRemoveBackground(const ElementContext& elementContext, const Vector<String>& types, const IPC::DataReference& data)
{
    send(Messages::WebPage::ReplaceImageForRemoveBackground(elementContext, types, data));
}

#endif

bool WebPageProxy::useGPUProcessForDOMRenderingEnabled() const
{
    if (id useGPUProcessForDOMRendering = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKit2GPUProcessForDOMRendering"])
        return [useGPUProcessForDOMRendering boolValue];

    return preferences().useGPUProcessForDOMRenderingEnabled();
}

} // namespace WebKit

#undef MESSAGE_CHECK_COMPLETION
#undef MESSAGE_CHECK
