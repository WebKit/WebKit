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
#import "APIUIClient.h"
#import "CocoaImage.h"
#import "Connection.h"
#import "DataDetectionResult.h"
#import "InsertTextOptions.h"
#import "LoadParameters.h"
#import "PageClient.h"
#import "QuickLookThumbnailLoader.h"
#import "SafeBrowsingSPI.h"
#import "SafeBrowsingWarning.h"
#import "SharedBufferCopy.h"
#import "SharedBufferDataReference.h"
#import "WebContextMenuProxy.h"
#import "WebPage.h"
#import "WebPageMessages.h"
#import "WebPasteboardProxy.h"
#import "WebProcessMessages.h"
#import "WebProcessProxy.h"
#import "WebsiteDataStore.h"
#import "WKErrorInternal.h"
#import <WebCore/DragItem.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/LocalCurrentGraphicsContext.h>
#import <WebCore/NetworkExtensionContentFilter.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/RunLoopObserver.h>
#import <WebCore/SearchPopupMenuCocoa.h>
#import <WebCore/TextAlternativeWithRange.h>
#import <WebCore/ValidationBubble.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/SoftLinking.h>
#import <wtf/cf/TypeCastsCF.h>

#if ENABLE(MEDIA_USAGE)
#import "MediaUsageManagerCocoa.h"
#endif

#if USE(APPKIT)
#import <AppKit/NSImage.h>
#else
#import <UIKit/UIImage.h>
#endif

#if PLATFORM(IOS)
#import <pal/spi/cocoa/WebFilterEvaluatorSPI.h>

SOFT_LINK_PRIVATE_FRAMEWORK(WebContentAnalysis);
SOFT_LINK_CLASS(WebContentAnalysis, WebFilterEvaluator);
#endif

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WebPageProxyCocoaAdditionsBefore.mm>
#endif

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WebPageProxyAdditions.h>
#else
#define WEB_PAGE_PROXY_ADDITIONS
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, process().connection())
#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, process().connection(), completion)

namespace WebKit {
using namespace WebCore;

#if ENABLE(DATA_DETECTION)
void WebPageProxy::setDataDetectionResult(const DataDetectionResult& dataDetectionResult)
{
    m_dataDetectionResults = dataDetectionResult.results;
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
    [context lookUpURL:url completionHandler:makeBlockPtr([listener = makeRef(listener), forMainFrameNavigation, url = url] (SSBLookupResult *result, NSError *error) mutable {
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
    if (WebFrameProxy* frame = process->webFrame(frameID))
        frame->contentFilterDidBlockLoad(unblockHandler);
}
#endif

void WebPageProxy::addPlatformLoadParameters(WebProcessProxy& process, LoadParameters& loadParameters)
{
    loadParameters.dataDetectionContext = m_uiClient->dataDetectionContext();

    loadParameters.networkExtensionSandboxExtensionHandles = createNetworkExtensionsSandboxExtensions(process);
    
#if PLATFORM(IOS)
    if (!process.hasManagedSessionSandboxAccess() && [getWebFilterEvaluatorClass() isManagedSession]) {
        SandboxExtension::Handle handle;
        SandboxExtension::createHandleForMachLookup("com.apple.uikit.viewservice.com.apple.WebContentFilter.remoteUI"_s, WTF::nullopt, handle);
        loadParameters.contentFilterExtensionHandle = WTFMove(handle);

        SandboxExtension::Handle frontboardServiceExtensionHandle;
        if (SandboxExtension::createHandleForMachLookup("com.apple.frontboard.systemappservices"_s, WTF::nullopt, frontboardServiceExtensionHandle))
            loadParameters.frontboardServiceExtensionHandle = WTFMove(frontboardServiceExtensionHandle);

        process.markHasManagedSessionSandboxAccess();
    }
#endif
}

void WebPageProxy::createSandboxExtensionsIfNeeded(const Vector<String>& files, SandboxExtension::Handle& fileReadHandle, SandboxExtension::HandleArray& fileUploadHandles)
{
    if (!files.size())
        return;

    if (files.size() == 1) {
        BOOL isDirectory;
        if ([[NSFileManager defaultManager] fileExistsAtPath:files[0] isDirectory:&isDirectory] && !isDirectory) {
            ASSERT(process().connection() && process().connection()->getAuditToken());
            if (process().connection() && process().connection()->getAuditToken())
                SandboxExtension::createHandleForReadByAuditToken("/", *(process().connection()->getAuditToken()), fileReadHandle);
            else
                SandboxExtension::createHandle("/", SandboxExtension::Type::ReadOnly, fileReadHandle);
            willAcquireUniversalFileReadSandboxExtension(m_process);
        }
    }

    fileUploadHandles.allocate(files.size());
    for (size_t i = 0; i< files.size(); i++) {
        NSString *file = files[i];
        if (![[NSFileManager defaultManager] fileExistsAtPath:file])
            continue;
        SandboxExtension::createHandle(file, SandboxExtension::Type::ReadOnly, fileUploadHandles[i]);
    }
}

bool WebPageProxy::scrollingUpdatesDisabledForTesting()
{
    return pageClient().scrollingUpdatesDisabledForTesting();
}

#if ENABLE(DRAG_SUPPORT)

void WebPageProxy::startDrag(const DragItem& dragItem, const ShareableBitmap::Handle& dragImageHandle)
{
    pageClient().startDrag(dragItem, dragImageHandle);
}

// FIXME: Move these functions to WebPageProxyIOS.mm.
#if PLATFORM(IOS_FAMILY)

void WebPageProxy::setPromisedDataForImage(const String&, const SharedMemory::IPCHandle&, const String&, const String&, const String&, const String&, const String&, const SharedMemory::IPCHandle&, const String&)
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

void WebPageProxy::platformRegisterAttachment(Ref<API::Attachment>&& attachment, const String& preferredFileName, const IPC::SharedBufferCopy& bufferCopy)
{
    if (bufferCopy.isEmpty())
        return;

    auto fileWrapper = adoptNS([pageClient().allocFileWrapperInstance() initRegularFileWithContents:bufferCopy.buffer()->createNSData().get()]);
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
    toAttachment->setFileWrapper(fromAttachment->fileWrapper());
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

void WebPageProxy::boundaryEventOccurred(WebCore::PlatformSpeechSynthesisUtterance&, WebCore::SpeechBoundary speechBoundary, unsigned charIndex)
{
    send(Messages::WebPage::BoundaryEventOccurred(speechBoundary == WebCore::SpeechBoundary::SpeechWordBoundary, charIndex));
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

void WebPageProxy::grantAccessToPreferenceService()
{
#if ENABLE(CFPREFS_DIRECT_MODE)
    process().unblockPreferenceServiceIfNeeded();
#endif
}

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

#if HAVE(QUICKLOOK_THUMBNAILING)

static RefPtr<WebKit::ShareableBitmap> convertPlatformImageToBitmap(CocoaImage *image, const WebCore::IntSize& fittingSize)
{
    FloatSize originalThumbnailSize([image size]);
    auto resultRect = roundedIntRect(largestRectWithAspectRatioInsideRect(originalThumbnailSize.aspectRatio(), { { }, fittingSize }));
    resultRect.setLocation({ });

    WebKit::ShareableBitmap::Configuration bitmapConfiguration;
    auto bitmap = WebKit::ShareableBitmap::createShareable(resultRect.size(), bitmapConfiguration);
    if (!bitmap)
        return nullptr;

    auto graphicsContext = bitmap->createGraphicsContext();
    if (!graphicsContext)
        return nullptr;

    LocalCurrentGraphicsContext savedContext(*graphicsContext);
#if PLATFORM(IOS_FAMILY)
    [image drawInRect:resultRect];
#elif USE(APPKIT)
    [image drawInRect:resultRect fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:YES hints:nil];
#endif

    return bitmap;
}

void WebPageProxy::requestThumbnailWithOperation(WKQLThumbnailLoadOperation *operation)
{
    [operation setCompletionBlock:^{
        RunLoop::main().dispatch([this, operation = retainPtr(operation)] {
            auto identifier = [operation identifier];
            auto convertedImage = convertPlatformImageToBitmap([operation thumbnail], WebCore::IntSize(400, 400));
            if (!convertedImage)
                return;
            this->updateAttachmentIcon(identifier, convertedImage);
        });
    }];
        
    [[WKQLThumbnailQueueManager sharedInstance].queue addOperation:operation];
}


void WebPageProxy::requestThumbnailWithFileWrapper(NSFileWrapper* fileWrapper, const String& identifier)
{
    auto operation = adoptNS([[WKQLThumbnailLoadOperation alloc] initWithAttachment:fileWrapper identifier:identifier]);
    requestThumbnailWithOperation(operation.get());
}

void WebPageProxy::requestThumbnailWithPath(const String& identifier, const String& filePath)
{
    auto operation = adoptNS([[WKQLThumbnailLoadOperation alloc] initWithURL:filePath identifier:identifier]);
    requestThumbnailWithOperation(operation.get());
    
}

#endif // HAVE(QUICKLOOK_THUMBNAILING)

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
        [CATransaction addCommitHandler:[weakThis = makeWeakPtr(*this)] {
            // We can't call dispatchActivityStateChange directly underneath this commit handler, because it has side-effects
            // that may result in other frameworks trying to install commit handlers for the same phase, which is not allowed.
            // So, dispatch_async here; we only care that the activity state change doesn't apply until after the active commit is complete.
            WorkQueue::main().dispatch([weakThis] {
                auto protectedThis = makeRefPtr(weakThis.get());
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

    send(Messages::WebPage::CreateAppHighlightInSelectedRange(createNewGroup, requestOriginatedInApp));
}

void WebPageProxy::restoreAppHighlightsAndScrollToIndex(const Vector<Ref<SharedMemory>>& highlights, const Optional<unsigned> index)
{
    if (!hasRunningProcess())
        return;

    Vector<SharedMemory::IPCHandle> memoryHandles;

    for (auto highlight : highlights) {
        SharedMemory::Handle handle;
        highlight->createHandle(handle, SharedMemory::Protection::ReadOnly);

        memoryHandles.append(SharedMemory::IPCHandle { WTFMove(handle), highlight->size() });
    }
    
    setUpHighlightsObserver();

    send(Messages::WebPage::RestoreAppHighlightsAndScrollToIndex(WTFMove(memoryHandles), index));
}

void WebPageProxy::setAppHighlightsVisibility(WebCore::HighlightVisibility appHighlightsVisibility)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetAppHighlightsVisibility(appHighlightsVisibility));
}

#endif

SandboxExtension::HandleArray WebPageProxy::createNetworkExtensionsSandboxExtensions(WebProcessProxy& process)
{
#if ENABLE(CONTENT_FILTERING)
    if (!process.hasNetworkExtensionSandboxAccess() && NetworkExtensionContentFilter::isRequired()) {
        process.markHasNetworkExtensionSandboxAccess();
        constexpr ASCIILiteral neHelperService { "com.apple.nehelper"_s };
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101500
        constexpr ASCIILiteral neSessionManagerService { "com.apple.nesessionmanager"_s };
#else
        constexpr ASCIILiteral neSessionManagerService { "com.apple.nesessionmanager.content-filter"_s };
#endif
        return SandboxExtension::createHandlesForMachLookup({ neHelperService, neSessionManagerService }, WTF::nullopt);
    }
#endif
    return SandboxExtension::HandleArray();
}

#if ENABLE(CONTEXT_MENUS)
#if ENABLE(IMAGE_EXTRACTION)

void WebPageProxy::handleContextMenuRevealImage()
{
    auto& result = m_activeContextMenuContextData.webHitTestResultData();
    if (!result.imageBitmap)
        return;

    revealExtractedImageInPreviewPanel(*result.imageBitmap, result.toolTipText);
}

#endif // ENABLE(IMAGE_EXTRACTION)

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

void WebPageProxy::setLastNavigationWasAppBound(ResourceRequest& request)
{
    WEB_PAGE_PROXY_ADDITIONS
    m_lastNavigationWasAppBound = request.isAppBound();
}

void WebPageProxy::lastNavigationWasAppBound(CompletionHandler<void(bool)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPage::LastNavigationWasAppBound(), WTFMove(completionHandler));
}

void WebPageProxy::grantAccessToAssetServices()
{
    SandboxExtension::Handle mobileAssetHandleV2;
    SandboxExtension::createHandleForMachLookup("com.apple.mobileassetd.v2"_s, WTF::nullopt, mobileAssetHandleV2);
    process().send(Messages::WebProcess::GrantAccessToAssetServices(mobileAssetHandleV2), 0);
}

void WebPageProxy::revokeAccessToAssetServices()
{
    process().send(Messages::WebProcess::RevokeAccessToAssetServices(), 0);
}

void WebPageProxy::switchFromStaticFontRegistryToUserFontRegistry()
{
    process().send(Messages::WebProcess::SwitchFromStaticFontRegistryToUserFontRegistry(fontdMachExtensionHandle()), 0);
}

SandboxExtension::Handle WebPageProxy::fontdMachExtensionHandle()
{
    SandboxExtension::Handle fontMachExtensionHandle;
    SandboxExtension::createHandleForMachLookup("com.apple.fonts"_s, WTF::nullopt, fontMachExtensionHandle);
    return fontMachExtensionHandle;
}

NSDictionary *WebPageProxy::contentsOfUserInterfaceItem(NSString *userInterfaceItem)
{
#if ENABLE(CONTEXT_MENUS)
    if (m_activeContextMenu && [userInterfaceItem isEqualToString:@"mediaControlsContextMenu"])
        return @{ userInterfaceItem: m_activeContextMenu->platformData() };
#endif // ENABLE(CONTEXT_MENUS)

    return nil;
}

} // namespace WebKit

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WebPageProxyCocoaAdditionsAfter.mm>
#endif

#undef MESSAGE_CHECK_COMPLETION
#undef MESSAGE_CHECK
