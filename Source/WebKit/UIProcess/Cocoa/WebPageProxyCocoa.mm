/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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
#import "APINavigation.h"
#import "APIPageConfiguration.h"
#import "APIUIClient.h"
#import "AppleMediaServicesUISPI.h"
#import "BrowsingWarning.h"
#import "CocoaImage.h"
#import "Connection.h"
#import "CoreTelephonyUtilities.h"
#import "DataDetectionResult.h"
#import "ExtensionCapabilityGranter.h"
#import "InsertTextOptions.h"
#import "LegacyWebArchiveCallbackAggregator.h"
#import "LoadParameters.h"
#import "MessageSenderInlines.h"
#import "NativeWebGestureEvent.h"
#import "NativeWebKeyboardEvent.h"
#import "NativeWebMouseEvent.h"
#import "NavigationState.h"
#import "PageClient.h"
#import "PlatformXRSystem.h"
#import "PlaybackSessionManagerProxy.h"
#import "RemoteLayerTreeCommitBundle.h"
#import "RemoteLayerTreeTransaction.h"
#import "SafeBrowsingSPI.h"
#import "SafeBrowsingUtilities.h"
#import "SharedBufferReference.h"
#import "SynapseSPI.h"
#import "VideoPresentationManagerProxy.h"
#import "WKErrorInternal.h"
#import "WKHistoryDelegatePrivate.h"
#import "WKWebView.h"
#import "WebContextMenuProxy.h"
#import "WebFrameProxy.h"
#import "WebPage.h"
#import "WebPageLoadTiming.h"
#import "WebPageMessages.h"
#import "WebPageProxyInternals.h"
#import "WebPasteboardProxy.h"
#import "WebPrivacyHelpers.h"
#import "WebProcessMessages.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import "WebScreenOrientationManagerProxy.h"
#import "WebsiteDataStore.h"
#import <Foundation/NSURLRequest.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/AppHighlight.h>
#import <WebCore/ApplePayAMSUIRequest.h>
#import <WebCore/DictationAlternative.h>
#import <WebCore/DragItem.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/HighlightVisibility.h>
#import <WebCore/LegacyWebArchive.h>
#import <WebCore/LocalCurrentGraphicsContext.h>
#import <WebCore/NetworkExtensionContentFilter.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/NowPlayingInfo.h>
#import <WebCore/NullPlaybackSessionInterface.h>
#import <WebCore/PlatformPlaybackSessionInterface.h>
#import <WebCore/PlaybackSessionInterfaceAVKitLegacy.h>
#import <WebCore/PlaybackSessionInterfaceMac.h>
#import <WebCore/PlaybackSessionInterfaceTVOS.h>
#import <WebCore/RunLoopObserver.h>
#import <WebCore/SearchPopupMenuCocoa.h>
#import <WebCore/SleepDisabler.h>
#import <WebCore/TextAlternativeWithRange.h>
#import <WebCore/TextAnimationTypes.h>
#import <WebCore/ValidationBubble.h>
#import <WebCore/VideoPresentationInterfaceIOS.h>
#import <WebCore/WebTextIndicatorLayer.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/ios/BrowserEngineKitSPI.h>
#import <pal/spi/mac/QuarantineSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/SoftLinking.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cf/VectorCF.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

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

#if HAVE(SCREEN_CAPTURE_KIT)
#import <WebCore/ScreenCaptureKitSharingSessionManager.h>
#endif

#if ENABLE(APPLE_PAY_AMS_UI)
SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(AppleMediaServices)
SOFT_LINK_CLASS_OPTIONAL(AppleMediaServices, AMSEngagementRequest)

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(AppleMediaServicesUI)
SOFT_LINK_CLASS_OPTIONAL(AppleMediaServicesUI, AMSUIEngagementTask)
#endif

#define MESSAGE_CHECK(assertion, connection) MESSAGE_CHECK_BASE(assertion, connection)
#define MESSAGE_CHECK_COMPLETION(assertion, connection, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, connection, completion)

#define WEBPAGEPROXY_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [pageProxyID=%llu, webPageID=%llu, PID=%i] WebPageProxy::" fmt, this, identifier().toUInt64(), webPageIDInMainFrameProcess().toUInt64(), m_legacyMainFrameProcess->processID(), ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

constexpr IntSize iconSize = IntSize(400, 400);

void WebPageProxy::didGeneratePageLoadTiming(const WebPageLoadTiming& timing)
{
    // These times will not exactly match times reported by the PLT benchmark, since the benchmark
    // uses loadRequestForNavigation as the start timestamp, while this object uses navigationStart
    // (didStartProvisionalLoadForFrameShared) as the start timestamp.
    auto url = m_mainFrame ? m_mainFrame->url() : URL();
    auto startTime = timing.navigationStart();
    auto firstVisualLayoutDuration = timing.firstVisualLayout() - startTime;
    auto firstMeaningfulPaintDuration = timing.firstMeaningfulPaint() - startTime;
    auto documentFinishedLoadingDuration = timing.documentFinishedLoading() - startTime;
    auto finishedLoadingDuration = timing.finishedLoading() - startTime;
    auto subresourcesFinishedLoadingDuration = timing.allSubresourcesFinishedLoading() - startTime;

    WEBPAGEPROXY_RELEASE_LOG(Loading, "didGeneratePageLoadTiming: url=%" SENSITIVE_LOG_STRING " firstVisualLayout=%.3f firstMeaningfulPaint=%.3f domContentLoaded=%.3f loadEvent=%.3f subresourcesFinished=%.3f", url.string().ascii().data(), firstVisualLayoutDuration.seconds(), firstMeaningfulPaintDuration.seconds(), documentFinishedLoadingDuration.seconds(), finishedLoadingDuration.seconds(), subresourcesFinishedLoadingDuration.seconds());

    static bool shouldLogFrameTree = CFPreferencesGetAppBooleanValue(CFSTR("WebKitDebugLogFrameTreesWithPageLoadTiming"), kCFPreferencesCurrentApplication, nullptr);
    if (shouldLogFrameTree)
        logFrameTree();

    if (RefPtr state = NavigationState::fromWebPage(*this))
        state->didGeneratePageLoadTiming(timing);
}

static bool exceedsRenderTreeSizeSizeThreshold(uint64_t thresholdSize, uint64_t committedSize)
{
    const double thesholdSizeFraction = 0.5; // Empirically-derived.
    return committedSize > thresholdSize * thesholdSizeFraction;
}

void WebPageProxy::didCommitLayerTree(const RemoteLayerTreeTransaction& layerTreeTransaction, const std::optional<MainFrameData>& mainFrameData, const PageData& pageData, const TransactionID& transactionID)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->didCommitLayerTree(layerTreeTransaction, mainFrameData, pageData, transactionID);

    // FIXME: Remove this special mechanism and fold it into the transaction's layout milestones.
    if (internals().observedLayoutMilestones.contains(WebCore::LayoutMilestone::ReachedSessionRestorationRenderTreeSizeThreshold) && !m_hitRenderTreeSizeThreshold
        && exceedsRenderTreeSizeSizeThreshold(m_sessionRestorationRenderTreeSize, pageData.renderTreeSize)) {
        m_hitRenderTreeSizeThreshold = true;
        didReachLayoutMilestone(WebCore::LayoutMilestone::ReachedSessionRestorationRenderTreeSizeThreshold, WallTime::now());
    }
}

void WebPageProxy::didCommitMainFrameData(const MainFrameData& mainFrameData, const TransactionID& transactionID)
{
    themeColorChanged(mainFrameData.themeColor);
    pageExtendedBackgroundColorDidChange(mainFrameData.pageExtendedBackgroundColor);
    sampledPageTopColorChanged(mainFrameData.sampledPageTopColor);

    if (!m_hasUpdatedRenderingAfterDidCommitLoad
        && (internals().firstLayerTreeTransactionIdAfterDidCommitLoad && transactionID.greaterThanOrEqualSameProcess(*internals().firstLayerTreeTransactionIdAfterDidCommitLoad))) {
        m_hasUpdatedRenderingAfterDidCommitLoad = true;
#if ENABLE(SCREEN_TIME)
        if (RefPtr pageClient = this->pageClient())
            pageClient->didChangeScreenTimeWebpageControllerURL();
#endif
        stopMakingViewBlankDueToLackOfRenderingUpdateIfNecessary();
        internals().lastVisibleContentRectUpdate = { };
    }

    if (std::exchange(internals().needsFixedContainerEdgesUpdateAfterNextCommit, false))
        protectedLegacyMainFrameProcess()->send(Messages::WebPage::SetNeedsFixedContainerEdgesUpdate(), webPageIDInMainFrameProcess());

    if (RefPtr pageClient = this->pageClient())
        pageClient->didCommitMainFrameData(mainFrameData);
}

void WebPageProxy::layerTreeCommitComplete()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->layerTreeCommitComplete();
}

#if ENABLE(DATA_DETECTION)

void WebPageProxy::setDataDetectionResult(DataDetectionResult&& dataDetectionResult)
{
    m_dataDetectionResults = createNSArray(dataDetectionResult.results, [](const RetainPtr<DDScannerResult>& result) {
        return result.get();
    });
}

void WebPageProxy::handleClickForDataDetectionResult(const DataDetectorElementInfo& info, const IntPoint& clickLocation)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->handleClickForDataDetectionResult(info, clickLocation);
}

#endif

void WebPageProxy::saveRecentSearches(IPC::Connection& connection, const String& name, const Vector<WebCore::RecentSearch>& searchItems)
{
    MESSAGE_CHECK(!name.isNull(), connection);

    protectedWebsiteDataStore()->saveRecentSearches(name, searchItems);
}

void WebPageProxy::loadRecentSearches(IPC::Connection& connection, const String& name, CompletionHandler<void(Vector<WebCore::RecentSearch>&&)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(!name.isNull(), connection, completionHandler({ }));

    protectedWebsiteDataStore()->loadRecentSearches(name, WTFMove(completionHandler));
}

std::optional<IPC::AsyncReplyID> WebPageProxy::grantAccessToCurrentPasteboardData(const String& pasteboardName, CompletionHandler<void()>&& completionHandler, std::optional<FrameIdentifier> frameID)
{
    if (!hasRunningProcess()) {
        completionHandler();
        return std::nullopt;
    }
    if (RefPtr frame = WebFrameProxy::webFrame(frameID))
        return WebPasteboardProxy::singleton().grantAccessToCurrentData(frame->protectedProcess(), pasteboardName, WTFMove(completionHandler));
    return WebPasteboardProxy::singleton().grantAccessToCurrentData(m_legacyMainFrameProcess, pasteboardName, WTFMove(completionHandler));
}

void WebPageProxy::beginSafeBrowsingCheck(const URL& url, API::Navigation& navigation, bool forMainFrameNavigation)
{
#if HAVE(SAFE_BROWSING)
    if (!SafeBrowsingUtilities::canLookUp(url))
        return;

    size_t redirectChainIndex = navigation.redirectChainIndex(url);

    navigation.setSafeBrowsingCheckOngoing(redirectChainIndex, true);

    auto performLookup = [weakThis = WeakPtr { *this }, navigation = Ref { navigation }, forMainFrameNavigation, url = url.isolatedCopy(), redirectChainIndex](RetainPtr<SSBLookupResult> cachedResult) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        auto navigationType = forMainFrameNavigation ? SafeBrowsingUtilities::NavigationType::MainFrame : SafeBrowsingUtilities::NavigationType::SubFrame;
        SafeBrowsingUtilities::lookUp(url, navigationType, cachedResult.get(), [weakThis = WTFMove(weakThis), navigation = WTFMove(navigation), forMainFrameNavigation, url = url.isolatedCopy(), redirectChainIndex](SSBLookupResult *result, NSError *error) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            navigation->setSafeBrowsingCheckOngoing(redirectChainIndex, false);
            if (error)
                return;

            RefPtr navigationState = NavigationState::fromWebPage(*protectedThis);
            auto historyDelegate = navigationState ? navigationState->historyDelegate() : nullptr;
            if (historyDelegate && [historyDelegate respondsToSelector:@selector(_webView:didReceiveSafeBrowsingResult:forURL:)]) {
                if (auto webView = protectedThis->cocoaView())
                    [historyDelegate _webView:webView.get() didReceiveSafeBrowsingResult:result forURL:url.createNSURL().get()];
            }

            for (SSBServiceLookupResult *lookupResult in [result serviceLookupResults]) {
                if (lookupResult.isPhishing || lookupResult.isMalware || lookupResult.isUnwantedSoftware) {
                    navigation->setSafeBrowsingWarning(BrowsingWarning::create(url, forMainFrameNavigation, BrowsingWarning::SafeBrowsingWarningData { lookupResult }));
                    break;
                }
            }

            if (!navigation->safeBrowsingCheckOngoing() && navigation->safeBrowsingWarning() && navigation->safeBrowsingCheckTimedOut())
                protectedThis->showBrowsingWarning(navigation->safeBrowsingWarning());
        });
    };

    RefPtr navigationState = NavigationState::fromWebPage(*this);
    auto historyDelegate = navigationState ? navigationState->historyDelegate() : nullptr;
    if (!historyDelegate || ![historyDelegate respondsToSelector:@selector(_webView:cachedSafeBrowsingResultForURL:completionHandler:)]) {
        performLookup(nullptr);
        return;
    }

    auto webView = cocoaView();
    auto cacheCompletionHandler = makeBlockPtr([performLookup = WTFMove(performLookup)] (SSBLookupResult *cachedResult, NSError *error) mutable {
        performLookup(retainPtr(cachedResult));
    });
    [historyDelegate _webView:webView.get() cachedSafeBrowsingResultForURL:url.createNSURL().get() completionHandler:cacheCompletionHandler.get()];
#endif
}

#if ENABLE(CONTENT_FILTERING)
void WebPageProxy::contentFilterDidBlockLoadForFrame(const WebCore::ContentFilterUnblockHandler& unblockHandler, FrameIdentifier frameID)
{
    contentFilterDidBlockLoadForFrameShared(unblockHandler, frameID);
}

void WebPageProxy::contentFilterDidBlockLoadForFrameShared(const WebCore::ContentFilterUnblockHandler& unblockHandler, FrameIdentifier frameID)
{
    if (RefPtr frame = WebFrameProxy::webFrame(frameID))
        frame->contentFilterDidBlockLoad(unblockHandler);
}
#endif

void WebPageProxy::addPlatformLoadParameters(WebProcessProxy& process, LoadParameters& loadParameters)
{
    loadParameters.dataDetectionReferenceDate = m_uiClient->dataDetectionReferenceDate();
}

void WebPageProxy::createSandboxExtensionsIfNeeded(const Vector<String>& files, SandboxExtension::Handle& fileReadHandle, Vector<SandboxExtension::Handle>& fileUploadHandles)
{
    WEBPAGEPROXY_RELEASE_LOG(Loading, "WebPageProxy::createSandboxExtensionsIfNeeded: %zu files", files.size());

    if (!files.size())
        return;

    auto createSandboxExtension = [protectedThis = Ref { *this }] (const String& path) {
        auto token = protectedThis->protectedLegacyMainFrameProcess()->protectedConnection()->getAuditToken();
        ASSERT(token);

        if (token) {
            if (auto handle = SandboxExtension::createHandleForReadByAuditToken(path, *token))
                return handle;
        }
        return SandboxExtension::createHandle(path, SandboxExtension::Type::ReadOnly);
    };

    if (files.size() == 1) {
        BOOL isDirectory;
        if ([[NSFileManager defaultManager] fileExistsAtPath:files[0].createNSString().get() isDirectory:&isDirectory] && !isDirectory) {
            if (auto handle = createSandboxExtension("/"_s))
                fileReadHandle = WTFMove(*handle);
            else if (auto handle = createSandboxExtension(files[0]))
                fileReadHandle = WTFMove(*handle);
            willAcquireUniversalFileReadSandboxExtension(m_legacyMainFrameProcess);
        }
    }

    for (auto& file : files) {
        if (![[NSFileManager defaultManager] fileExistsAtPath:file.createNSString().get()])
            continue;
        if (auto handle = createSandboxExtension(file))
            fileUploadHandles.append(WTFMove(*handle));
    }
}

void WebPageProxy::scrollingNodeScrollViewDidScroll(ScrollingNodeID nodeID)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->scrollingNodeScrollViewDidScroll(nodeID);
}

bool WebPageProxy::scrollingUpdatesDisabledForTesting()
{
    RefPtr pageClient = this->pageClient();
    return pageClient && pageClient->scrollingUpdatesDisabledForTesting();
}

#if ENABLE(DRAG_SUPPORT)

void WebPageProxy::startDrag(const DragItem& dragItem, ShareableBitmap::Handle&& dragImageHandle, const std::optional<NodeIdentifier>& nodeID)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->startDrag(dragItem, WTFMove(dragImageHandle), nodeID);
}

#endif

#if ENABLE(ATTACHMENT_ELEMENT)

void WebPageProxy::platformRegisterAttachment(Ref<API::Attachment>&& attachment, const String& preferredFileName, const IPC::SharedBufferReference& bufferCopy)
{
    if (bufferCopy.isEmpty())
        return;

    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    // FIXME: This is a safer cpp false positive.
    SUPPRESS_RETAINPTR_CTOR_ADOPT RetainPtr fileWrapper = adoptNS([pageClient->allocFileWrapperInstance() initRegularFileWithContents:bufferCopy.unsafeBuffer()->createNSData().get()]);
    [fileWrapper setPreferredFilename:preferredFileName.createNSString().get()];
    attachment->setFileWrapper(fileWrapper.get());
}

void WebPageProxy::platformRegisterAttachment(Ref<API::Attachment>&& attachment, const String& filePath)
{
    if (!filePath)
        return;

    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    // FIXME: This is a safer cpp false positive.
    SUPPRESS_RETAINPTR_CTOR_ADOPT RetainPtr fileWrapper = adoptNS([pageClient->allocFileWrapperInstance() initWithURL:adoptNS([[NSURL alloc] initFileURLWithPath:filePath.createNSString().get()]).get() options:0 error:nil]);
    attachment->setFileWrapper(fileWrapper.get());
}

void WebPageProxy::platformCloneAttachment(Ref<API::Attachment>&& fromAttachment, Ref<API::Attachment>&& toAttachment)
{
    fromAttachment->cloneFileWrapperTo(toAttachment);
}

static RefPtr<WebCore::ShareableBitmap> convertPlatformImageToBitmap(CocoaImage *image, const WebCore::FloatSize& fittingSize)
{
    FloatSize originalThumbnailSize([image size]);
    if (originalThumbnailSize.isEmpty())
        return nullptr;

    auto resultRect = roundedIntRect(largestRectWithAspectRatioInsideRect(originalThumbnailSize.aspectRatio(), { { }, fittingSize }));
    resultRect.setLocation({ });

    auto bitmap = WebCore::ShareableBitmap::create({ resultRect.size() });
    if (!bitmap)
        return nullptr;

    auto graphicsContext = bitmap->createGraphicsContext();
    if (!graphicsContext)
        return nullptr;

    LocalCurrentGraphicsContext savedContext(*graphicsContext);
    [image drawInRect:resultRect];

    return bitmap;
}

RefPtr<WebCore::ShareableBitmap> WebPageProxy::iconForAttachment(const String& fileName, const String& contentType, const String& title, FloatSize& size)
{
#if PLATFORM(IOS_FAMILY)
    auto iconAndSize = RenderThemeIOS::iconForAttachment(fileName, contentType, title);
#else
    auto iconAndSize = RenderThemeMac::iconForAttachment(fileName, contentType, title);
#endif
    auto icon = iconAndSize.icon;
    size = iconAndSize.size;
    return convertPlatformImageToBitmap(icon.get(), iconSize);
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

void WebPageProxy::performDictionaryLookupAtLocation(const WebCore::FloatPoint& point)
{
    if (!hasRunningProcess())
        return;
    
    protectedLegacyMainFrameProcess()->send(Messages::WebPage::PerformDictionaryLookupAtLocation(point), webPageIDInMainFrameProcess());
}

void WebPageProxy::insertDictatedTextAsync(const String& text, const EditingRange& replacementRange, const Vector<TextAlternativeWithRange>& dictationAlternativesWithRange, InsertTextOptions&& options)
{
    if (!hasRunningProcess())
        return;

    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    Vector<DictationAlternative> dictationAlternatives;
    for (const auto& alternativeWithRange : dictationAlternativesWithRange) {
        if (auto context = pageClient->addDictationAlternatives(alternativeWithRange.alternatives.get()))
            dictationAlternatives.append({ alternativeWithRange.range, *context });
    }

    if (dictationAlternatives.isEmpty()) {
        insertTextAsync(text, replacementRange, WTFMove(options));
        return;
    }

    protectedLegacyMainFrameProcess()->send(Messages::WebPage::InsertDictatedTextAsync { text, replacementRange, dictationAlternatives, WTFMove(options) }, webPageIDInMainFrameProcess());
}

void WebPageProxy::addDictationAlternative(TextAlternativeWithRange&& alternative)
{
    if (!hasRunningProcess())
        return;

    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    RetainPtr nsAlternatives = alternative.alternatives.get();
    auto context = pageClient->addDictationAlternatives(nsAlternatives.get());
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::AddDictationAlternative { nsAlternatives.get().primaryString, *context }, [context, weakThis = WeakPtr { *this }](bool success) {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && !success)
            protectedThis->removeDictationAlternatives(*context);
    }, webPageIDInMainFrameProcess());
}

void WebPageProxy::dictationAlternativesAtSelection(CompletionHandler<void(Vector<DictationContext>&&)>&& completion)
{
    if (!hasRunningProcess()) {
        completion({ });
        return;
    }

    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::DictationAlternativesAtSelection(), WTFMove(completion), webPageIDInMainFrameProcess());
}

void WebPageProxy::clearDictationAlternatives(Vector<DictationContext>&& alternativesToClear)
{
    if (!hasRunningProcess() || alternativesToClear.isEmpty())
        return;

    protectedLegacyMainFrameProcess()->send(Messages::WebPage::ClearDictationAlternatives(WTFMove(alternativesToClear)), webPageIDInMainFrameProcess());
}

ResourceError WebPageProxy::errorForUnpermittedAppBoundDomainNavigation(const URL& url)
{
    return { WKErrorDomain, WKErrorNavigationAppBoundDomain, url, localizedDescriptionForErrorCode(WKErrorNavigationAppBoundDomain).get() };
}

WebPageProxy::Internals::~Internals() = default;

#if ENABLE(APPLE_PAY)

std::optional<SharedPreferencesForWebProcess> WebPageProxy::Internals::sharedPreferencesForWebPaymentMessages() const
{
    return protectedPage()->protectedLegacyMainFrameProcess()->sharedPreferencesForWebProcess();
}

IPC::Connection* WebPageProxy::Internals::paymentCoordinatorConnection(const WebPaymentCoordinatorProxy&)
{
    return &page->legacyMainFrameProcess().connection();
}

const String& WebPageProxy::Internals::paymentCoordinatorBoundInterfaceIdentifier(const WebPaymentCoordinatorProxy&)
{
    return page->websiteDataStore().configuration().boundInterfaceIdentifier();
}

void WebPageProxy::Internals::getPaymentCoordinatorEmbeddingUserAgent(WebPageProxyIdentifier, CompletionHandler<void(const String&)>&& completionHandler)
{
    completionHandler(page->userAgent());
}

CocoaWindow *WebPageProxy::Internals::paymentCoordinatorPresentingWindow(const WebPaymentCoordinatorProxy&) const
{
    RefPtr pageClient = protectedPage()->pageClient();
    return pageClient ? pageClient->platformWindow() : nullptr;
}

const String& WebPageProxy::Internals::paymentCoordinatorSourceApplicationBundleIdentifier(const WebPaymentCoordinatorProxy&)
{
    return page->websiteDataStore().configuration().sourceApplicationBundleIdentifier();
}

const String& WebPageProxy::Internals::paymentCoordinatorSourceApplicationSecondaryIdentifier(const WebPaymentCoordinatorProxy&)
{
    return page->websiteDataStore().configuration().sourceApplicationSecondaryIdentifier();
}

void WebPageProxy::Internals::paymentCoordinatorAddMessageReceiver(WebPaymentCoordinatorProxy&, IPC::ReceiverName receiverName, IPC::MessageReceiver& messageReceiver)
{
    protectedPage()->protectedLegacyMainFrameProcess()->addMessageReceiver(receiverName, page->webPageIDInMainFrameProcess(), messageReceiver);
}

void WebPageProxy::Internals::paymentCoordinatorRemoveMessageReceiver(WebPaymentCoordinatorProxy&, IPC::ReceiverName receiverName)
{
    protectedPage()->protectedLegacyMainFrameProcess()->removeMessageReceiver(receiverName, page->webPageIDInMainFrameProcess());
}

#endif // ENABLE(APPLE_PAY)

#if ENABLE(SPEECH_SYNTHESIS)

void WebPageProxy::Internals::didStartSpeaking(WebCore::PlatformSpeechSynthesisUtterance&)
{
    if (speechSynthesisData().speakingStartedCompletionHandler)
        speechSynthesisData().speakingStartedCompletionHandler();
}

void WebPageProxy::Internals::didFinishSpeaking(WebCore::PlatformSpeechSynthesisUtterance&)
{
    if (speechSynthesisData().speakingFinishedCompletionHandler)
        speechSynthesisData().speakingFinishedCompletionHandler();
}

void WebPageProxy::Internals::didPauseSpeaking(WebCore::PlatformSpeechSynthesisUtterance&)
{
    if (speechSynthesisData().speakingPausedCompletionHandler)
        speechSynthesisData().speakingPausedCompletionHandler();
}

void WebPageProxy::Internals::didResumeSpeaking(WebCore::PlatformSpeechSynthesisUtterance&)
{
    if (speechSynthesisData().speakingResumedCompletionHandler)
        speechSynthesisData().speakingResumedCompletionHandler();
}

void WebPageProxy::Internals::speakingErrorOccurred(WebCore::PlatformSpeechSynthesisUtterance&)
{
    Ref protectedPage = page.get();
    protectedPage->protectedLegacyMainFrameProcess()->send(Messages::WebPage::SpeakingErrorOccurred(), protectedPage->webPageIDInMainFrameProcess());
}

void WebPageProxy::Internals::boundaryEventOccurred(WebCore::PlatformSpeechSynthesisUtterance&, WebCore::SpeechBoundary speechBoundary, unsigned charIndex, unsigned charLength)
{
    Ref protectedPage = page.get();
    protectedPage->protectedLegacyMainFrameProcess()->send(Messages::WebPage::BoundaryEventOccurred(speechBoundary == WebCore::SpeechBoundary::SpeechWordBoundary, charIndex, charLength), protectedPage->webPageIDInMainFrameProcess());
}

void WebPageProxy::Internals::voicesDidChange()
{
    Ref protectedPage = page.get();
    protectedPage->protectedLegacyMainFrameProcess()->send(Messages::WebPage::VoicesDidChange(), protectedPage->webPageIDInMainFrameProcess());
}

#endif // ENABLE(SPEECH_SYNTHESIS)

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
void WebPageProxy::didCreateContextInWebProcessForVisibilityPropagation(LayerHostingContextID contextID)
{
    m_contextIDForVisibilityPropagationInWebProcess = contextID;
    if (RefPtr pageClient = this->pageClient())
        pageClient->didCreateContextInWebProcessForVisibilityPropagation(contextID);
}

#if ENABLE(GPU_PROCESS)
void WebPageProxy::didCreateContextInGPUProcessForVisibilityPropagation(LayerHostingContextID contextID)
{
    m_contextIDForVisibilityPropagationInGPUProcess = contextID;
    if (RefPtr pageClient = this->pageClient())
        pageClient->didCreateContextInGPUProcessForVisibilityPropagation(contextID);
}
#endif // ENABLE(GPU_PROCESS)

#if ENABLE(MODEL_PROCESS)
void WebPageProxy::didCreateContextInModelProcessForVisibilityPropagation(LayerHostingContextID contextID)
{
    m_contextIDForVisibilityPropagationInModelProcess = contextID;
    if (RefPtr pageClient = this->pageClient())
        pageClient->didCreateContextInModelProcessForVisibilityPropagation(contextID);
}
#endif // ENABLE(MODEL_PROCESS)
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

#if PLATFORM(VISION)
void WebPageProxy::enterExternalPlaybackForNowPlayingMediaSession(CompletionHandler<void(bool, UIViewController *)>&& enterHandler, CompletionHandler<void(bool)>&& exitHandler)
{
    if (!m_videoPresentationManager) {
        enterHandler(false, nil);
        exitHandler(false);
        return;
    }

    RefPtr videoPresentationInterface = m_videoPresentationManager->controlsManagerInterface();
    if (!videoPresentationInterface) {
        enterHandler(false, nil);
        exitHandler(false);
        return;
    }

    videoPresentationInterface->enterExternalPlayback(WTFMove(enterHandler), WTFMove(exitHandler));
}

void WebPageProxy::exitExternalPlayback()
{
    if (!m_videoPresentationManager)
        return;

    RefPtr videoPresentationInterface = m_videoPresentationManager->controlsManagerInterface();
    if (!videoPresentationInterface)
        return;

    videoPresentationInterface->exitExternalPlayback();
}
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)

void WebPageProxy::didChangePlaybackRate(PlaybackSessionContextIdentifier identifier)
{
    if (internals().currentFullscreenVideoSessionIdentifier == identifier)
        updateFullscreenVideoTextRecognition();
}

void WebPageProxy::didChangeCurrentTime(PlaybackSessionContextIdentifier identifier)
{
    if (internals().currentFullscreenVideoSessionIdentifier == identifier)
        updateFullscreenVideoTextRecognition();
}

void WebPageProxy::updateFullscreenVideoTextRecognition()
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient || !pageClient->isTextRecognitionInFullscreenVideoEnabled())
        return;

    RefPtr playbackSessionManager = m_playbackSessionManager;
    if (internals().currentFullscreenVideoSessionIdentifier && playbackSessionManager && playbackSessionManager->isPaused(*internals().currentFullscreenVideoSessionIdentifier)) {
        internals().fullscreenVideoTextRecognitionTimer.startOneShot(250_ms);
        return;
    }

    internals().fullscreenVideoTextRecognitionTimer.stop();

    if (!internals().currentFullscreenVideoSessionIdentifier)
        return;

#if PLATFORM(IOS_FAMILY)
    if (RetainPtr controller = m_videoPresentationManager->playerViewController(*internals().currentFullscreenVideoSessionIdentifier))
        pageClient->cancelTextRecognitionForFullscreenVideo(controller.get());
#endif
}

void WebPageProxy::fullscreenVideoTextRecognitionTimerFired()
{
    if (!internals().currentFullscreenVideoSessionIdentifier || !m_videoPresentationManager)
        return;

    auto identifier = *internals().currentFullscreenVideoSessionIdentifier;
    RefPtr { m_videoPresentationManager }->requestBitmapImageForCurrentTime(identifier, [identifier, weakThis = WeakPtr { *this }](std::optional<ShareableBitmap::Handle>&& imageHandle) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || protectedThis->internals().currentFullscreenVideoSessionIdentifier != identifier)
            return;

        RefPtr presentationManager = protectedThis->m_videoPresentationManager;
        if (!presentationManager)
            return;
        if (!imageHandle)
            return;

#if PLATFORM(IOS_FAMILY)
        RetainPtr controller = presentationManager->playerViewController(identifier);
        if (!controller)
            return;
        if (RefPtr pageClient = protectedThis->pageClient())
            pageClient->beginTextRecognitionForFullscreenVideo(WTFMove(*imageHandle), controller.get());
#endif
    });
}
#endif // ENABLE(VIDEO_PRESENTATION_MODE)

#if ENABLE(ATTACHMENT_ELEMENT) && PLATFORM(MAC)

bool WebPageProxy::updateIconForDirectory(NSFileWrapper *fileWrapper, const String& identifier)
{
    RetainPtr image = [fileWrapper icon];
    if (!image)
        return false;

    auto convertedImage = convertPlatformImageToBitmap(image.get(), iconSize);
    if (!convertedImage)
        return false;

    auto handle = convertedImage->createHandle();
    if (!handle)
        return false;
    protectedLegacyMainFrameProcess()->send(Messages::WebPage::UpdateAttachmentIcon(identifier, WTFMove(handle), iconSize), webPageIDInMainFrameProcess());
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
            WorkQueue::mainSingleton().dispatch([weakThis] {
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

void WebPageProxy::createTextFragmentDirectiveFromSelection(CompletionHandler<void(URL&&)>&& completionHandler)
{
    if (!hasRunningProcess())
        return;

    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::CreateTextFragmentDirectiveFromSelection(), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::getTextFragmentRanges(CompletionHandler<void(const Vector<EditingRange>&&)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler({ });
        return;
    }

    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::GetTextFragmentRanges(), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

#if ENABLE(APP_HIGHLIGHTS)
void WebPageProxy::createAppHighlightInSelectedRange(WebCore::CreateNewGroupForHighlight createNewGroup, WebCore::HighlightRequestOriginatedInApp requestOriginatedInApp)
{
    if (!hasRunningProcess())
        return;

    setUpHighlightsObserver();

    auto completionHandler = [this, protectedThis = Ref { *this }] (WebCore::AppHighlight&& highlight) {
        // FIXME: Make a way to get the IPC::Connection that sent the reply in the CompletionHandler.
        MESSAGE_CHECK_BASE(!highlight.highlight->isEmpty(), legacyMainFrameProcess().connection());
        if (RefPtr pageClient = this->pageClient())
            pageClient->storeAppHighlight(highlight);
    };
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::CreateAppHighlightInSelectedRange(createNewGroup, requestOriginatedInApp), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::restoreAppHighlightsAndScrollToIndex(const Vector<Ref<SharedMemory>>& highlights, const std::optional<unsigned> index)
{
    if (!hasRunningProcess())
        return;

    auto memoryHandles = WTF::compactMap(highlights, [](auto& highlight) {
        return highlight->createHandle(SharedMemory::Protection::ReadOnly);
    });
    
    setUpHighlightsObserver();

    protectedLegacyMainFrameProcess()->send(Messages::WebPage::RestoreAppHighlightsAndScrollToIndex(WTFMove(memoryHandles), index), webPageIDInMainFrameProcess());
}

void WebPageProxy::setAppHighlightsVisibility(WebCore::HighlightVisibility appHighlightsVisibility)
{
    RELEASE_ASSERT(isMainRunLoop());
    
    if (!hasRunningProcess())
        return;

    protectedLegacyMainFrameProcess()->send(Messages::WebPage::SetAppHighlightsVisibility(appHighlightsVisibility), webPageIDInMainFrameProcess());
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

    // FIXME: When in element fullscreen, UIClient::presentingViewController() may not return the
    // WKFullScreenViewController even though that is the presenting view controller of the WKWebView.
    // We should call PageClientImpl::presentingViewController() instead.
    RetainPtr presentingViewController = uiClient().presentingViewController();
    if (!presentingViewController) {
        completionHandler(std::nullopt);
        return;
    }

    RetainPtr amsRequest = adoptNS([allocAMSEngagementRequestInstance() initWithRequestDictionary:dynamic_objc_cast<NSDictionary>([NSJSONSerialization JSONObjectWithData:retainPtr([request.engagementRequest.createNSString() dataUsingEncoding:NSUTF8StringEncoding]).get() options:0 error:nil])]);
    [amsRequest setOriginatingURL:originatingURL.createNSURL().get()];

    auto amsBag = retainPtr([getAMSUIEngagementTaskClassSingleton() createBagForSubProfile]);

    m_applePayAMSUISession = adoptNS([allocAMSUIEngagementTaskInstance() initWithRequest:amsRequest.get() bag:amsBag.get() presentingViewController:presentingViewController.get()]);
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

#if ENABLE(CONTEXT_MENUS)

#if HAVE(TRANSLATION_UI_SERVICES)

bool WebPageProxy::canHandleContextMenuTranslation() const
{
    RefPtr pageClient = this->pageClient();
    return pageClient && pageClient->canHandleContextMenuTranslation();
}

void WebPageProxy::handleContextMenuTranslation(const TranslationContextMenuInfo& info)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->handleContextMenuTranslation(info);
}

#endif // HAVE(TRANSLATION_UI_SERVICES)

#if ENABLE(WRITING_TOOLS)

bool WebPageProxy::canHandleContextMenuWritingTools() const
{
    RefPtr pageClient = this->pageClient();
    return pageClient && pageClient->canHandleContextMenuWritingTools();
}

#endif // ENABLE(WRITING_TOOLS)

#endif // ENABLE(CONTEXT_MENUS)

void WebPageProxy::requestActiveNowPlayingSessionInfo(CompletionHandler<void(bool, WebCore::NowPlayingInfo&&)>&& callback)
{
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::RequestActiveNowPlayingSessionInfo(), WTFMove(callback), webPageIDInMainFrameProcess());
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
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::LastNavigationWasAppInitiated(), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::grantAccessToAssetServices()
{
    auto handles = SandboxExtension::createHandlesForMachLookup({ "com.apple.mobileassetd.v2"_s }, protectedLegacyMainFrameProcess()->auditToken(), SandboxExtension::MachBootstrapOptions::EnableMachBootstrap);
    protectedLegacyMainFrameProcess()->send(Messages::WebProcess::GrantAccessToAssetServices(WTFMove(handles)), 0);
}

void WebPageProxy::revokeAccessToAssetServices()
{
    protectedLegacyMainFrameProcess()->send(Messages::WebProcess::RevokeAccessToAssetServices(), 0);
}

void WebPageProxy::disableURLSchemeCheckInDataDetectors() const
{
    protectedLegacyMainFrameProcess()->send(Messages::WebProcess::DisableURLSchemeCheckInDataDetectors(), 0);
}

void WebPageProxy::switchFromStaticFontRegistryToUserFontRegistry()
{
    if (auto handles = protectedLegacyMainFrameProcess()->fontdMachExtensionHandles())
        protectedLegacyMainFrameProcess()->send(Messages::WebProcess::SwitchFromStaticFontRegistryToUserFontRegistry(WTFMove(*handles)), 0);
}

NSDictionary *WebPageProxy::contentsOfUserInterfaceItem(NSString *userInterfaceItem)
{
#if ENABLE(CONTEXT_MENUS)
    RefPtr activeContextMenu = m_activeContextMenu;
    if (activeContextMenu && [userInterfaceItem isEqualToString:@"mediaControlsContextMenu"])
        return @{ userInterfaceItem: activeContextMenu->platformData().get() };
#endif // ENABLE(CONTEXT_MENUS)

    return nil;
}

#if PLATFORM(MAC)
bool WebPageProxy::isQuarantinedAndNotUserApproved(const String& fileURLString)
{
    RetainPtr fileURL = adoptNS([[NSURL alloc] initWithString:fileURLString.createNSString().get()]);
    if ([retainPtr(fileURL.get().pathExtension) caseInsensitiveCompare:@"webarchive"] != NSOrderedSame)
        return false;

    qtn_file_t qf = qtn_file_alloc();

    int quarantineError = qtn_file_init_with_path(qf, fileURL.get().path.fileSystemRepresentation);

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

#if ENABLE(MULTI_REPRESENTATION_HEIC)

void WebPageProxy::insertMultiRepresentationHEIC(NSData *data, NSString *altText)
{
    protectedLegacyMainFrameProcess()->send(Messages::WebPage::InsertMultiRepresentationHEIC(span(data), altText), webPageIDInMainFrameProcess());
}

#endif

void WebPageProxy::replaceSelectionWithPasteboardData(const Vector<String>& types, std::span<const uint8_t> data)
{
    protectedLegacyMainFrameProcess()->send(Messages::WebPage::ReplaceSelectionWithPasteboardData(types, data), webPageIDInMainFrameProcess());
}

RetainPtr<WKWebView> WebPageProxy::cocoaView()
{
    return internals().cocoaView.get();
}

void WebPageProxy::setCocoaView(WKWebView *view)
{
    internals().cocoaView = view;
}

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

void WebPageProxy::replaceImageForRemoveBackground(const ElementContext& elementContext, const Vector<String>& types, std::span<const uint8_t> data)
{
    protectedLegacyMainFrameProcess()->send(Messages::WebPage::ReplaceImageForRemoveBackground(elementContext, types, data), webPageIDInMainFrameProcess());
}

#endif

bool WebPageProxy::useGPUProcessForDOMRenderingEnabled() const
{
    if (RetainPtr useGPUProcessForDOMRendering = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKit2GPUProcessForDOMRendering"])
        return [useGPUProcessForDOMRendering boolValue];

    if (protectedPreferences()->useGPUProcessForDOMRenderingEnabled())
        return true;

    Ref configuration = m_configuration;
#if ENABLE(REMOTE_LAYER_TREE_ON_MAC_BY_DEFAULT)
    if (configuration->lockdownModeEnabled())
        return true;
#endif

    HashSet<RefPtr<const WebPageProxy>> visitedPages;
    visitedPages.add(this);
    for (RefPtr page = configuration->relatedPage(); page && !visitedPages.contains(page); page = page->configuration().relatedPage()) {
        if (page->protectedPreferences()->useGPUProcessForDOMRenderingEnabled())
            return true;
        visitedPages.add(page);
    }

    return false;
}

bool WebPageProxy::shouldForceForegroundPriorityForClientNavigation() const
{
    // The client may request that we do client navigations at foreground priority, even if the
    // view is not visible, as long as the application is foreground.
    if (!configuration().clientNavigationsRunAtForegroundPriority())
        return false;

    // This setting only applies to background views. There is no need to force foreground
    // priority for foreground views since they get foreground priority by virtue of being
    // visible.
    if (isViewVisible())
        return false;

    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return false;

    bool canTakeForegroundAssertions = pageClient->canTakeForegroundAssertions();
    WEBPAGEPROXY_RELEASE_LOG(Process, "WebPageProxy::shouldForceForegroundPriorityForClientNavigation() returns %d based on PageClient::canTakeForegroundAssertions()", canTakeForegroundAssertions);
    return canTakeForegroundAssertions;
}

#if HAVE(ESIM_AUTOFILL_SYSTEM_SUPPORT)

bool WebPageProxy::shouldAllowAutoFillForCellularIdentifiers() const
{
    return WebKit::shouldAllowAutoFillForCellularIdentifiers(URL { pageLoadState().activeURL() });
}

#endif

#if ENABLE(EXTENSION_CAPABILITIES)

const MediaCapability* WebPageProxy::mediaCapability() const
{
    return internals().mediaCapability.get();
}

void WebPageProxy::setMediaCapability(RefPtr<MediaCapability>&& capability)
{
    if (RefPtr oldCapability = std::exchange(internals().mediaCapability, nullptr))
        deactivateMediaCapability(*oldCapability);

    internals().mediaCapability = WTFMove(capability);

    if (!internals().mediaCapability) {
        WEBPAGEPROXY_RELEASE_LOG(ProcessCapabilities, "setMediaCapability: clearing media capability");
        protectedLegacyMainFrameProcess()->send(Messages::WebPage::SetMediaEnvironment({ }), webPageIDInMainFrameProcess());
        return;
    }

    WEBPAGEPROXY_RELEASE_LOG(ProcessCapabilities, "setMediaCapability: creating (envID=%{public}s) for URL '%{sensitive}s'", internals().mediaCapability->environmentIdentifier().utf8().data(), internals().mediaCapability->webPageURL().string().utf8().data());
    protectedLegacyMainFrameProcess()->send(Messages::WebPage::SetMediaEnvironment(internals().mediaCapability->environmentIdentifier()), webPageIDInMainFrameProcess());
}

void WebPageProxy::deactivateMediaCapability(MediaCapability& capability)
{
    WEBPAGEPROXY_RELEASE_LOG(ProcessCapabilities, "deactivateMediaCapability: deactivating (envID=%{public}s) for URL '%{sensitive}s'", capability.environmentIdentifier().utf8().data(), capability.webPageURL().string().utf8().data());
    Ref processPool { protectedLegacyMainFrameProcess()->protectedProcessPool() };
    processPool->extensionCapabilityGranter().setMediaCapabilityActive(capability, false);
    processPool->extensionCapabilityGranter().revoke(capability, *this);
}

void WebPageProxy::resetMediaCapability()
{
    if (!preferences().mediaCapabilityGrantsEnabled())
        return;

    URL currentURL { this->currentURL() };

    if (!hasRunningProcess() || !currentURL.isValid()) {
        setMediaCapability(nullptr);
        return;
    }

    RefPtr mediaCapability = this->mediaCapability();
    if (!mediaCapability || !protocolHostAndPortAreEqual(mediaCapability->webPageURL(), currentURL))
        setMediaCapability(MediaCapability::create(WTFMove(currentURL)));
}

void WebPageProxy::updateMediaCapability()
{
    RefPtr mediaCapability = internals().mediaCapability;
    if (!mediaCapability)
        return;

    if (shouldDeactivateMediaCapability()) {
        deactivateMediaCapability(*mediaCapability);
        return;
    }

    Ref processPool { protectedLegacyMainFrameProcess()->protectedProcessPool() };

    if (shouldActivateMediaCapability())
        processPool->extensionCapabilityGranter().setMediaCapabilityActive(*mediaCapability, true);

    if (mediaCapability->isActivatingOrActive())
        processPool->extensionCapabilityGranter().grant(*mediaCapability, *this);
}

bool WebPageProxy::shouldActivateMediaCapability() const
{
    if (!isViewVisible())
        return false;

    return MediaProducer::needsMediaCapability(internals().mediaState);
}

bool WebPageProxy::shouldDeactivateMediaCapability() const
{
    RefPtr mediaCapability = this->mediaCapability();
    if (!mediaCapability || !mediaCapability->isActivatingOrActive())
        return false;

    if (internals().mediaState & WebCore::MediaProducer::MediaCaptureMask)
        return false;

    if (internals().mediaState.containsAny(MediaProducerMediaState::HasAudioOrVideo))
        return false;

    if (hasValidAudibleActivity())
        return false;

    return true;
}

#endif // ENABLE(EXTENSION_CAPABILITIES)

#if ENABLE(WRITING_TOOLS)

void WebPageProxy::setWritingToolsActive(bool active)
{
    if (m_isWritingToolsActive == active)
        return;

    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    pageClient->writingToolsActiveWillChange();
    m_isWritingToolsActive = active;
    pageClient->writingToolsActiveDidChange();
}

WebCore::WritingTools::Behavior WebPageProxy::writingToolsBehavior() const
{
    if (isEditable())
        return WebCore::WritingTools::Behavior::Complete;

    auto& editorState = this->editorState();
    auto& configuration = this->configuration();

    if (configuration.writingToolsBehavior() == WebCore::WritingTools::Behavior::None || editorState.selectionIsNone || editorState.isInPasswordField || editorState.isInPlugin)
        return WebCore::WritingTools::Behavior::None;

    if (configuration.writingToolsBehavior() == WebCore::WritingTools::Behavior::Complete && editorState.isContentEditable)
        return WebCore::WritingTools::Behavior::Complete;

    return WebCore::WritingTools::Behavior::Limited;
}

void WebPageProxy::willBeginWritingToolsSession(const std::optional<WebCore::WritingTools::Session>& session, CompletionHandler<void(const Vector<WebCore::WritingTools::Context>&)>&& completionHandler)
{
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::WillBeginWritingToolsSession(session), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::didBeginWritingToolsSession(const WebCore::WritingTools::Session& session, const Vector<WebCore::WritingTools::Context>& contexts)
{
    protectedLegacyMainFrameProcess()->send(Messages::WebPage::DidBeginWritingToolsSession(session, contexts), webPageIDInMainFrameProcess());
}

void WebPageProxy::proofreadingSessionDidReceiveSuggestions(const WebCore::WritingTools::Session& session, const Vector<WebCore::WritingTools::TextSuggestion>& suggestions, const WebCore::CharacterRange& processedRange, const WebCore::WritingTools::Context& context, bool finished, CompletionHandler<void()>&& completionHandler)
{
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::ProofreadingSessionDidReceiveSuggestions(session, suggestions, processedRange, context, finished), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::proofreadingSessionDidUpdateStateForSuggestion(const WebCore::WritingTools::Session& session, WebCore::WritingTools::TextSuggestion::State state, const WebCore::WritingTools::TextSuggestion& suggestion, const WebCore::WritingTools::Context& context)
{
    protectedLegacyMainFrameProcess()->send(Messages::WebPage::ProofreadingSessionDidUpdateStateForSuggestion(session, state, suggestion, context), webPageIDInMainFrameProcess());
}

void WebPageProxy::willEndWritingToolsSession(const WebCore::WritingTools::Session& session, bool accepted, CompletionHandler<void()>&& completionHandler)
{
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::WillEndWritingToolsSession(session, accepted), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::didEndWritingToolsSession(const WebCore::WritingTools::Session& session, bool accepted)
{
    protectedLegacyMainFrameProcess()->send(Messages::WebPage::DidEndWritingToolsSession(session, accepted), webPageIDInMainFrameProcess());
}

void WebPageProxy::compositionSessionDidReceiveTextWithReplacementRange(const WebCore::WritingTools::Session& session, const WebCore::AttributedString& attributedText, const WebCore::CharacterRange& range, const WebCore::WritingTools::Context& context, bool finished, CompletionHandler<void()>&& completionHandler)
{
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::CompositionSessionDidReceiveTextWithReplacementRange(session, attributedText, range, context, finished), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::writingToolsSessionDidReceiveAction(const WebCore::WritingTools::Session& session, WebCore::WritingTools::Action action)
{
    protectedLegacyMainFrameProcess()->send(Messages::WebPage::WritingToolsSessionDidReceiveAction(session, action), webPageIDInMainFrameProcess());
}

void WebPageProxy::proofreadingSessionSuggestionTextRectsInRootViewCoordinates(const WebCore::CharacterRange& enclosingRangeRelativeToSessionRange, CompletionHandler<void(Vector<WebCore::FloatRect>&&)>&& completionHandler) const
{
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::ProofreadingSessionSuggestionTextRectsInRootViewCoordinates(enclosingRangeRelativeToSessionRange), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::updateTextVisibilityForActiveWritingToolsSession(const WebCore::CharacterRange& rangeRelativeToSessionRange, bool visible, const WTF::UUID& identifier, CompletionHandler<void()>&& completionHandler)
{
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::UpdateTextVisibilityForActiveWritingToolsSession(rangeRelativeToSessionRange, visible, identifier), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::textPreviewDataForActiveWritingToolsSession(const WebCore::CharacterRange& rangeRelativeToSessionRange, CompletionHandler<void(RefPtr<WebCore::TextIndicator>&&)>&& completionHandler)
{
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::TextPreviewDataForActiveWritingToolsSession(rangeRelativeToSessionRange), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::decorateTextReplacementsForActiveWritingToolsSession(const WebCore::CharacterRange& rangeRelativeToSessionRange, CompletionHandler<void()>&& completionHandler)
{
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::DecorateTextReplacementsForActiveWritingToolsSession(rangeRelativeToSessionRange), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::setSelectionForActiveWritingToolsSession(const WebCore::CharacterRange& rangeRelativeToSessionRange, CompletionHandler<void()>&& completionHandler)
{
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::SetSelectionForActiveWritingToolsSession(rangeRelativeToSessionRange), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::addTextAnimationForAnimationID(IPC::Connection& connection, const WTF::UUID& uuid, const WebCore::TextAnimationData& styleData, const RefPtr<WebCore::TextIndicator> textIndicator)
{
    addTextAnimationForAnimationIDWithCompletionHandler(connection, uuid, styleData, textIndicator, { });
}

void WebPageProxy::addTextAnimationForAnimationIDWithCompletionHandler(IPC::Connection& connection, const WTF::UUID& uuid, const WebCore::TextAnimationData& styleData, const RefPtr<WebCore::TextIndicator> textIndicator, CompletionHandler<void(WebCore::TextAnimationRunMode)>&& completionHandler)
{
    if (completionHandler)
        MESSAGE_CHECK_COMPLETION(uuid.isValid(), connection, completionHandler({ }));
    else
        MESSAGE_CHECK(uuid.isValid(), connection);

    internals().textIndicatorDataForAnimationID.add(uuid, textIndicator);

    if (completionHandler)
        internals().completionHandlerForAnimationID.add(uuid, WTFMove(completionHandler));

#if PLATFORM(IOS_FAMILY)
    // The shape of the iOS API requires us to have stored this completionHandler when we call into the WebProcess
    // to replace the text and generate the text indicator of the replacement text.
    if (auto destinationAnimationCompletionHandler = internals().completionHandlerForDestinationTextIndicatorForSourceID.take(uuid))
        destinationAnimationCompletionHandler(textIndicator->data());

    // Storing and sending information for the different shaped SPI on iOS.
    if (styleData.runMode == WebCore::TextAnimationRunMode::RunAnimation) {
        if (styleData.style == WebCore::TextAnimationType::Source)
            internals().sourceAnimationIDtoDestinationAnimationID.add(*styleData.destinationAnimationUUID, uuid);

        if (styleData.style == WebCore::TextAnimationType::Final) {
            if (auto sourceAnimationID = internals().sourceAnimationIDtoDestinationAnimationID.take(uuid)) {
                if (auto completionHandler = internals().completionHandlerForDestinationTextIndicatorForSourceID.take(sourceAnimationID))
                    completionHandler(textIndicator->data());
            }
        }
    }
#endif

    if (RefPtr pageClient = this->pageClient())
        pageClient->addTextAnimationForAnimationID(uuid, styleData);
}

void WebPageProxy::callCompletionHandlerForAnimationID(const WTF::UUID& uuid, WebCore::TextAnimationRunMode runMode)
{
    if (!hasRunningProcess())
        return;

    if (auto completionHandler = internals().completionHandlerForAnimationID.take(uuid))
        completionHandler(runMode);
}

#if PLATFORM(IOS_FAMILY)
void WebPageProxy::storeDestinationCompletionHandlerForAnimationID(const WTF::UUID& destinationAnimationUUID, CompletionHandler<void(std::optional<WebCore::TextIndicatorData>)>&& completionHandler)
{
    internals().completionHandlerForDestinationTextIndicatorForSourceID.add(destinationAnimationUUID, WTFMove(completionHandler));
}
#endif

void WebPageProxy::getTextIndicatorForID(const WTF::UUID& uuid, CompletionHandler<void(RefPtr<WebCore::TextIndicator>&&)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler(nullptr);
        return;
    }

    RefPtr textIndicator = internals().textIndicatorDataForAnimationID.get(uuid);

    if (textIndicator) {
        completionHandler(WTFMove(textIndicator));
        return;
    }

    // FIXME: This shouldn't be reached/called anymore. Verify and remove.
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::CreateTextIndicatorForTextAnimationID(uuid), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::updateUnderlyingTextVisibilityForTextAnimationID(const WTF::UUID& uuid, bool visible, CompletionHandler<void()>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler();
        return;
    }

    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::UpdateUnderlyingTextVisibilityForTextAnimationID(uuid, visible), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::didEndPartialIntelligenceTextAnimationImpl()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->didEndPartialIntelligenceTextAnimation();
}

void WebPageProxy::didEndPartialIntelligenceTextAnimation(IPC::Connection&)
{
    didEndPartialIntelligenceTextAnimationImpl();
}

bool WebPageProxy::writingToolsTextReplacementsFinished()
{
    if (RefPtr pageClient = this->pageClient())
        return pageClient->writingToolsTextReplacementsFinished();
    return true;
}

void WebPageProxy::intelligenceTextAnimationsDidComplete()
{
    if (!hasRunningProcess())
        return;

    protectedLegacyMainFrameProcess()->send(Messages::WebPage::IntelligenceTextAnimationsDidComplete(), webPageIDInMainFrameProcess());
}

void WebPageProxy::removeTextAnimationForAnimationID(IPC::Connection& connection, const WTF::UUID& uuid)
{
    MESSAGE_CHECK(uuid.isValid(), connection);

    if (RefPtr pageClient = this->pageClient())
        pageClient->removeTextAnimationForAnimationID(uuid);
}

void WebPageProxy::proofreadingSessionShowDetailsForSuggestionWithIDRelativeToRect(IPC::Connection& connection, const WebCore::WritingTools::TextSuggestion::ID& replacementID, WebCore::IntRect selectionBoundsInRootView)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->proofreadingSessionShowDetailsForSuggestionWithIDRelativeToRect(replacementID, selectionBoundsInRootView);
}

void WebPageProxy::proofreadingSessionUpdateStateForSuggestionWithID(IPC::Connection& connection, WebCore::WritingTools::TextSuggestion::State state, const WebCore::WritingTools::TextSuggestion::ID& replacementID)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->proofreadingSessionUpdateStateForSuggestionWithID(state, replacementID);
}

#endif // ENABLE(WRITING_TOOLS)

void WebPageProxy::createTextIndicatorForElementWithID(const String& elementID, CompletionHandler<void(RefPtr<WebCore::TextIndicator>&&)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler(nil);
        return;
    }

    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::CreateTextIndicatorForElementWithID(elementID), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::setTextIndicatorFromFrame(FrameIdentifier frameID, const RefPtr<WebCore::TextIndicator>&& textIndicator, WebCore::TextIndicatorLifetime lifetime)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    if (!textIndicator)
        return;

    auto rect = textIndicator->textBoundingRectInRootViewCoordinates();
    convertRectToMainFrameCoordinates(rect, frame->rootFrame().frameID(), [weakThis = WeakPtr { *this }, textIndicator = WTFMove(textIndicator), lifetime] (std::optional<FloatRect> convertedRect) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !convertedRect)
            return;
        textIndicator->setTextBoundingRectInRootViewCoordinates(*convertedRect);
        protectedThis->setTextIndicator(WTFMove(textIndicator), lifetime);
    });
}

void WebPageProxy::setTextIndicator(RefPtr<WebCore::TextIndicator>&& textIndicator, WebCore::TextIndicatorLifetime lifetime)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    RetainPtr<CALayer> installationLayer = pageClient->textIndicatorInstallationLayer();

    teardownTextIndicatorLayer();
    m_textIndicatorFadeTimer.stop();

    m_textIndicator = textIndicator;

    CGRect frame = m_textIndicator->textBoundingRectInRootViewCoordinates();
    m_textIndicatorLayer = adoptNS([[WebTextIndicatorLayer alloc] initWithFrame:frame
        textIndicator:m_textIndicator margin:CGSizeZero offset:CGPointZero]);

    [installationLayer addSublayer:m_textIndicatorLayer.get()];

    if (m_textIndicator->presentationTransition() != WebCore::TextIndicatorPresentationTransition::None)
        [m_textIndicatorLayer present];

    if ((TextIndicatorLifetime)lifetime == TextIndicatorLifetime::Temporary)
        m_textIndicatorFadeTimer.startOneShot(WebCore::timeBeforeFadeStarts);
}

void WebPageProxy::updateTextIndicatorFromFrame(FrameIdentifier frameID, RefPtr<WebCore::TextIndicator>&& textIndicator)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    if (!textIndicator)
        return;

    auto rect = textIndicator->textBoundingRectInRootViewCoordinates();
    convertRectToMainFrameCoordinates(rect, frame->rootFrame().frameID(), [weakThis = WeakPtr { *this }, textIndicator = WTFMove(textIndicator)] (std::optional<FloatRect> convertedRect) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !convertedRect)
            return;
        textIndicator->setTextBoundingRectInRootViewCoordinates(*convertedRect);
        protectedThis->updateTextIndicator(WTFMove(textIndicator));
    });
}

void WebPageProxy::updateTextIndicator(RefPtr<WebCore::TextIndicator>&& textIndicator)
{
    if (m_textIndicator && m_textIndicatorLayer)
        [m_textIndicatorLayer updateWithFrame:m_textIndicator->textBoundingRectInRootViewCoordinates() textIndicator:textIndicator.get() margin:CGSizeZero offset:CGPointZero updatingIndicator:YES];
}

void WebPageProxy::clearTextIndicator()
{
    clearTextIndicatorWithAnimation(WebCore::TextIndicatorDismissalAnimation::FadeOut);
}

void WebPageProxy::clearTextIndicatorWithAnimation(WebCore::TextIndicatorDismissalAnimation animation)
{
    if ([m_textIndicatorLayer isFadingOut])
        return;

    RefPtr textIndicator = m_textIndicator;

    if (textIndicator && textIndicator->wantsManualAnimation() && [m_textIndicatorLayer hasCompletedAnimation] && animation == WebCore::TextIndicatorDismissalAnimation::FadeOut) {
        startTextIndicatorFadeOut();
        return;
    }

    teardownTextIndicatorLayer();
}

void WebPageProxy::setTextIndicatorAnimationProgress(float animationProgress)
{
    if (!m_textIndicator)
        return;

    [m_textIndicatorLayer setAnimationProgress:animationProgress];
}

void WebPageProxy::teardownTextIndicatorLayer()
{
    [m_textIndicatorLayer removeFromSuperlayer];
    m_textIndicatorLayer = nil;
}

void WebPageProxy::startTextIndicatorFadeOut()
{
    [m_textIndicatorLayer setFadingOut:YES];

    [m_textIndicatorLayer hideWithCompletionHandler:[weakThis = WeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        protectedThis->teardownTextIndicatorLayer();
    }];
}

#if ENABLE(VIDEO_PRESENTATION_MODE)

void WebPageProxy::playPredominantOrNowPlayingMediaSession(CompletionHandler<void(bool)>&& completion)
{
    if (tryToSendCommandToActiveControlledVideo(PlatformMediaSession::RemoteControlCommandType::PlayCommand)) {
        completion(true);
        return;
    }

    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::StartPlayingPredominantVideo(), WTFMove(completion), webPageIDInMainFrameProcess());
}

void WebPageProxy::pauseNowPlayingMediaSession(CompletionHandler<void(bool)>&& completion)
{
    completion(tryToSendCommandToActiveControlledVideo(PlatformMediaSession::RemoteControlCommandType::PauseCommand));
}

bool WebPageProxy::tryToSendCommandToActiveControlledVideo(PlatformMediaSession::RemoteControlCommandType command)
{
    if (!hasActiveVideoForControlsManager())
        return false;

    WeakPtr model = protectedPlaybackSessionManager()->controlsManagerInterface()->playbackSessionModel();
    if (!model)
        return false;

    model->sendRemoteCommand(command, { });
    return true;
}

#endif // ENABLE(VIDEO_PRESENTATION_MODE)

void WebPageProxy::getInformationFromImageData(Vector<uint8_t>&& data, CompletionHandler<void(Expected<std::pair<String, Vector<IntSize>>, WebCore::ImageDecodingError>&&)>&& completionHandler)
{
    if (isClosed())
        return completionHandler(makeUnexpected(WebCore::ImageDecodingError::Internal));

    ensureProtectedRunningProcess()->sendWithAsyncReply(Messages::WebPage::GetInformationFromImageData(WTFMove(data)), [preventProcessShutdownScope = protectedLegacyMainFrameProcess()->shutdownPreventingScope(), completionHandler = WTFMove(completionHandler)] (auto result) mutable {
        completionHandler(WTFMove(result));
    }, webPageIDInMainFrameProcess());
}

void WebPageProxy::createIconDataFromImageData(Ref<WebCore::SharedBuffer>&& buffer, const Vector<unsigned>& lengths, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&& completionHandler)
{
    if (isClosed())
        return completionHandler(nullptr);

    // Supported ICO image sizes by ImageIO.
    constexpr std::array<unsigned, 5> availableLengths { { 16, 32, 48, 128, 256 } };
    auto targetLengths = lengths.isEmpty() ? std::span { availableLengths } : lengths;

    ensureProtectedRunningProcess()->sendWithAsyncReply(Messages::WebPage::CreateBitmapsFromImageData(WTFMove(buffer), targetLengths), [preventProcessShutdownScope = protectedLegacyMainFrameProcess()->shutdownPreventingScope(), completionHandler = WTFMove(completionHandler)] (auto bitmaps) mutable {
        if (bitmaps.isEmpty())
            return completionHandler(nullptr);

        completionHandler(createIconDataFromBitmaps(WTFMove(bitmaps)));
    }, webPageIDInMainFrameProcess());
}

void WebPageProxy::decodeImageData(Ref<WebCore::SharedBuffer>&& buffer, std::optional<WebCore::FloatSize> preferredSize, CompletionHandler<void(RefPtr<WebCore::ShareableBitmap>&&)>&& completionHandler)
{
    if (isClosed())
        return completionHandler(nullptr);

    ensureProtectedRunningProcess()->sendWithAsyncReply(Messages::WebPage::DecodeImageData(WTFMove(buffer), preferredSize), [preventProcessShutdownScope = protectedLegacyMainFrameProcess()->shutdownPreventingScope(), completionHandler = WTFMove(completionHandler)] (auto result) mutable {
        completionHandler(WTFMove(result));
    }, webPageIDInMainFrameProcess());
}

void WebPageProxy::getWebArchiveData(CompletionHandler<void(API::Data*)>&& completionHandler)
{
    RefPtr mainFrame = m_mainFrame;
    if (!mainFrame) {
        // Return blank page data for backforward compatibility; see rdar://127469660.
        launchInitialProcessIfNecessary();
        protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::GetWebArchiveData(), [completionHandler = WTFMove(completionHandler)](auto&& result) mutable {
            if (!result)
                return completionHandler(nullptr);

            completionHandler(API::Data::create(result->span()).ptr());
        }, webPageIDInMainFrameProcess());
        return;
    }

    getWebArchiveDataWithFrame(*mainFrame, WTFMove(completionHandler));
}

void WebPageProxy::getWebArchiveDataWithFrame(WebFrameProxy& frame, CompletionHandler<void(API::Data*)>&& completionHandler)
{
    return getWebArchiveDataWithSelectedFrames(frame, std::nullopt, WTFMove(completionHandler));
}

void WebPageProxy::getWebArchiveDataWithSelectedFrames(WebFrameProxy& rootFrame, const std::optional<HashSet<WebCore::FrameIdentifier>>& selectedFrameIdentifiers, CompletionHandler<void(API::Data*)>&& completionHandler)
{
    if (selectedFrameIdentifiers && !selectedFrameIdentifiers->contains(rootFrame.frameID()))
        return completionHandler(nullptr);

    auto callbackAggregator = LegacyWebArchiveCallbackAggregator::create(rootFrame.frameID(), { }, [completionHandler = WTFMove(completionHandler)](auto webArchive) mutable {
        if (!webArchive)
            return completionHandler(nullptr);

        RetainPtr data = webArchive->rawDataRepresentation();
        if (!data)
            return completionHandler(nullptr);
        completionHandler(API::Data::create(span(data.get())).ptr());
    });
    HashMap<Ref<WebProcessProxy>, Vector<WebCore::FrameIdentifier>> processFrames;
    RefPtr currentFrame = &rootFrame;
    while (currentFrame) {
        if (!selectedFrameIdentifiers || selectedFrameIdentifiers->contains(currentFrame->frameID())) {
            processFrames.ensure(currentFrame->protectedProcess(), [&] {
                return Vector<WebCore::FrameIdentifier> { };
            }).iterator->value.append(currentFrame->frameID());
        }

        currentFrame = currentFrame->traverseNext().frame;
    }

    for (auto& [process, frameIDs] : processFrames) {
        Ref { process }->sendWithAsyncReply(Messages::WebPage::GetWebArchivesForFrames(frameIDs), [frameIDs, callbackAggregator](auto&& result) {
            if (result.size() > frameIDs.size())
                return;

            callbackAggregator->addResult(WTFMove(result));
        }, webPageIDInProcess(process.get()));
    }
}

String WebPageProxy::presentingApplicationBundleIdentifier() const
{
    if (std::optional auditToken = presentingApplicationAuditToken()) {
        NSError *error = nil;
        RetainPtr bundleProxy = [LSBundleProxy bundleProxyWithAuditToken:*auditToken error:&error];
        if (error)
            RELEASE_LOG_ERROR(WebRTC, "Failed to get attribution bundleID from audit token with error: %@.", error.localizedDescription);
        else
            return bundleProxy.get().bundleIdentifier;
    }
#if PLATFORM(MAC)
    else
        return [NSRunningApplication currentApplication].bundleIdentifier;
#endif

    return { };
}

#if PLATFORM(MAC)
NSDictionary *WebPageProxy::getAccessibilityWebProcessDebugInfo()
{
    const Seconds messageTimeout(2);
    auto sendResult = protectedLegacyMainFrameProcess()->sendSync(Messages::WebPage::GetAccessibilityWebProcessDebugInfo(), webPageIDInMainFrameProcess(), messageTimeout);

    if (!sendResult.succeeded())
        return @{ };

    auto [result] = sendResult.takeReplyOr(WebCore::AXDebugInfo({ 0, 0 }));

    return @{
        @"axIsEnabled": [NSNumber numberWithBool:result.isAccessibilityEnabled],
        @"axIsThreadInitialized": [NSNumber numberWithBool:result.isAccessibilityThreadInitialized],
        @"axLiveTree": result.liveTree.createNSString().get(),
        @"axIsolatedTree": result.isolatedTree.createNSString().get(),
        @"axWebProcessRemoteHash": [NSNumber numberWithUnsignedInteger:result.remoteTokenHash],
        @"axWebProcessLocalHash": [NSNumber numberWithUnsignedInteger:result.webProcessLocalTokenHash]
    };
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void WebPageProxy::clearAccessibilityIsolatedTree()
{
    forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.send(Messages::WebPage::ClearAccessibilityIsolatedTree(), pageID);
    });
}
#endif
#endif // PLATFORM(MAC)

} // namespace WebKit

#undef MESSAGE_CHECK_COMPLETION
#undef MESSAGE_CHECK
