/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "PageClientImplCocoa.h"

#import "APIUIClient.h"
#import "RemoteLayerTreeCommitBundle.h"
#import "RemoteLayerTreeTransaction.h"
#import "WKWebViewInternal.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageProxy.h"
#import <WebCore/AlternativeTextUIController.h>
#import <WebCore/FixedContainerEdges.h>
#import <WebCore/TextAnimationTypes.h>
#import <WebCore/WritingToolsTypes.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <pal/spi/ios/BrowserEngineKitSPI.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/WTFString.h>

#if ENABLE(SCREEN_TIME)
#import <pal/cocoa/ScreenTimeSoftLink.h>
#endif

namespace WebKit {

PageClientImplCocoa::PageClientImplCocoa(WKWebView *webView)
    : m_webView { webView }
    , m_alternativeTextUIController { makeUnique<WebCore::AlternativeTextUIController>() }
{
}

PageClientImplCocoa::~PageClientImplCocoa() = default;

void PageClientImplCocoa::obscuredContentInsetsDidChange()
{
    RetainPtr webView = m_webView.get();
    [webView _recalculateViewportSizesWithMinimumViewportInset:[webView minimumViewportInset] maximumViewportInset:[webView maximumViewportInset] throwOnInvalidInput:NO];
}

void PageClientImplCocoa::themeColorWillChange()
{
    [webView() willChangeValueForKey:@"themeColor"];
}

void PageClientImplCocoa::themeColorDidChange()
{
    [webView() didChangeValueForKey:@"themeColor"];
}

void PageClientImplCocoa::underPageBackgroundColorWillChange()
{
    [webView() willChangeValueForKey:@"underPageBackgroundColor"];
}

void PageClientImplCocoa::underPageBackgroundColorDidChange()
{
    RetainPtr webView = this->webView();

    [webView didChangeValueForKey:@"underPageBackgroundColor"];
#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    [webView _updateTopScrollPocketCaptureColor];
#endif
}

void PageClientImplCocoa::sampledPageTopColorWillChange()
{
    [webView() willChangeValueForKey:@"_sampledPageTopColor"];
}

void PageClientImplCocoa::sampledPageTopColorDidChange()
{
    [webView() didChangeValueForKey:@"_sampledPageTopColor"];
}

#if ENABLE(WEB_PAGE_SPATIAL_BACKDROP)
void PageClientImplCocoa::spatialBackdropSourceWillChange()
{
    [webView() willChangeValueForKey:@"_spatialBackdropSource"];
}

void PageClientImplCocoa::spatialBackdropSourceDidChange()
{
    RetainPtr webView = m_webView.get();
    [webView _spatialBackdropSourceDidChange];
    [webView didChangeValueForKey:@"_spatialBackdropSource"];
}
#endif

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
void PageClientImplCocoa::canEnterImmersiveElementFromURL(const URL& url, CompletionHandler<void(bool)>&& completion)
{
    [m_webView _canEnterImmersiveElementFromURL:url completion:WTFMove(completion)];
}
#endif

void PageClientImplCocoa::isPlayingAudioWillChange()
{
    [webView() willChangeValueForKey:RetainPtr { NSStringFromSelector(@selector(_isPlayingAudio)) }.get()];
}

void PageClientImplCocoa::isPlayingAudioDidChange()
{
    [webView() didChangeValueForKey:RetainPtr { NSStringFromSelector(@selector(_isPlayingAudio)) }.get()];
}

bool PageClientImplCocoa::scrollingUpdatesDisabledForTesting()
{
    return [webView() _scrollingUpdatesDisabledForTesting];
}

#if ENABLE(ATTACHMENT_ELEMENT)

void PageClientImplCocoa::didInsertAttachment(API::Attachment& attachment, const String& source)
{
    [webView() _didInsertAttachment:attachment withSource:source.createNSString().get()];
}

void PageClientImplCocoa::didRemoveAttachment(API::Attachment& attachment)
{
    [webView() _didRemoveAttachment:attachment];
}

void PageClientImplCocoa::didInvalidateDataForAttachment(API::Attachment& attachment)
{
    [webView() _didInvalidateDataForAttachment:attachment];
}

NSFileWrapper *PageClientImplCocoa::allocFileWrapperInstance() const
{
    RetainPtr cls = [webView() configuration]._attachmentFileWrapperClass ?: [NSFileWrapper class];
    SUPPRESS_RETAINPTR_CTOR_ADOPT return [cls.get() alloc];
}

NSSet *PageClientImplCocoa::serializableFileWrapperClasses() const
{
    RetainPtr<Class> defaultFileWrapperClass = NSFileWrapper.class;
    RetainPtr<Class> configuredFileWrapperClass = [webView() configuration]._attachmentFileWrapperClass;
    if (configuredFileWrapperClass && configuredFileWrapperClass.get() != defaultFileWrapperClass.get())
        return [NSSet setWithObjects:configuredFileWrapperClass.get(), defaultFileWrapperClass.get(), nil];
    return [NSSet setWithObjects:defaultFileWrapperClass.get(), nil];
}

#endif

#if ENABLE(APP_HIGHLIGHTS)
void PageClientImplCocoa::storeAppHighlight(const WebCore::AppHighlight &highlight)
{
    [webView() _storeAppHighlight:highlight];
}
#endif // ENABLE(APP_HIGHLIGHTS)

void PageClientImplCocoa::pageClosed()
{
    m_alternativeTextUIController->clear();
}

#if ENABLE(GPU_PROCESS)
void PageClientImplCocoa::gpuProcessDidFinishLaunching()
{
    RetainPtr webView = m_webView.get();
    [webView willChangeValueForKey:@"_gpuProcessIdentifier"];
    [webView didChangeValueForKey:@"_gpuProcessIdentifier"];
}

void PageClientImplCocoa::gpuProcessDidExit()
{
    RetainPtr webView = m_webView.get();
    [webView willChangeValueForKey:@"_gpuProcessIdentifier"];
    [webView didChangeValueForKey:@"_gpuProcessIdentifier"];
}
#endif

#if ENABLE(MODEL_PROCESS)
void PageClientImplCocoa::modelProcessDidFinishLaunching()
{
    RetainPtr webView = m_webView.get();
    [webView willChangeValueForKey:@"_modelProcessIdentifier"];
    [webView didChangeValueForKey:@"_modelProcessIdentifier"];
}

void PageClientImplCocoa::modelProcessDidExit()
{
    RetainPtr webView = m_webView.get();
    [webView willChangeValueForKey:@"_modelProcessIdentifier"];
    [webView didChangeValueForKey:@"_modelProcessIdentifier"];
}
#endif

std::optional<WebCore::DictationContext> PageClientImplCocoa::addDictationAlternatives(PlatformTextAlternatives *alternatives)
{
    return m_alternativeTextUIController->addAlternatives(alternatives);
}

void PageClientImplCocoa::replaceDictationAlternatives(PlatformTextAlternatives *alternatives, WebCore::DictationContext context)
{
    m_alternativeTextUIController->replaceAlternatives(alternatives, context);
}

void PageClientImplCocoa::removeDictationAlternatives(WebCore::DictationContext dictationContext)
{
    m_alternativeTextUIController->removeAlternatives(dictationContext);
}

Vector<String> PageClientImplCocoa::dictationAlternatives(WebCore::DictationContext dictationContext)
{
    return makeVector<String>(retainPtr(RetainPtr { platformDictationAlternatives(dictationContext) }.get().alternativeStrings).get());
}

PlatformTextAlternatives *PageClientImplCocoa::platformDictationAlternatives(WebCore::DictationContext dictationContext)
{
    return m_alternativeTextUIController->alternativesForContext(dictationContext);
}

void PageClientImplCocoa::microphoneCaptureWillChange()
{
    [webView() willChangeValueForKey:@"microphoneCaptureState"];
}

void PageClientImplCocoa::cameraCaptureWillChange()
{
    [webView() willChangeValueForKey:@"cameraCaptureState"];
}

void PageClientImplCocoa::displayCaptureWillChange()
{
    [webView() willChangeValueForKey:@"_displayCaptureState"];
}

void PageClientImplCocoa::displayCaptureSurfacesWillChange()
{
    [webView() willChangeValueForKey:@"_displayCaptureSurfaces"];
}

void PageClientImplCocoa::systemAudioCaptureWillChange()
{
    [webView() willChangeValueForKey:@"_systemAudioCaptureState"];
}

void PageClientImplCocoa::microphoneCaptureChanged()
{
    [webView() didChangeValueForKey:@"microphoneCaptureState"];
}

void PageClientImplCocoa::cameraCaptureChanged()
{
    [webView() didChangeValueForKey:@"cameraCaptureState"];
}

void PageClientImplCocoa::displayCaptureChanged()
{
    [webView() didChangeValueForKey:@"_displayCaptureState"];
}

void PageClientImplCocoa::displayCaptureSurfacesChanged()
{
    [webView() didChangeValueForKey:@"_displayCaptureSurfaces"];
}

void PageClientImplCocoa::systemAudioCaptureChanged()
{
    [webView() didChangeValueForKey:@"_systemAudioCaptureState"];
}

WindowKind PageClientImplCocoa::windowKind()
{
    RetainPtr window = [webView() window];
    if (!window)
        return WindowKind::Unparented;
    if ([window isKindOfClass:NSClassFromString(@"_SCNSnapshotWindow")])
        return WindowKind::InProcessSnapshotting;
    return WindowKind::Normal;
}

#if ENABLE(WRITING_TOOLS)
void PageClientImplCocoa::proofreadingSessionShowDetailsForSuggestionWithIDRelativeToRect(const WebCore::WritingTools::TextSuggestion::ID& replacementID, WebCore::IntRect selectionBoundsInRootView)
{
    [webView() _proofreadingSessionShowDetailsForSuggestionWithUUID:replacementID.createNSUUID().get() relativeToRect:selectionBoundsInRootView];
}

void PageClientImplCocoa::proofreadingSessionUpdateStateForSuggestionWithID(WebCore::WritingTools::TextSuggestion::State state, const WebCore::WritingTools::TextSuggestion::ID& replacementID)
{
    [webView() _proofreadingSessionUpdateState:state forSuggestionWithUUID:replacementID.createNSUUID().get()];
}

static NSString *writingToolsActiveKeySingleton()
{
    static NeverDestroyed<RetainPtr<NSString>> writingToolsActiveKey = retainPtr(@"writingToolsActive");
    return writingToolsActiveKey.get().get();
}

void PageClientImplCocoa::writingToolsActiveWillChange()
{
    [webView() willChangeValueForKey:writingToolsActiveKeySingleton()];
}

void PageClientImplCocoa::writingToolsActiveDidChange()
{
    [webView() didChangeValueForKey:writingToolsActiveKeySingleton()];
}

void PageClientImplCocoa::didEndPartialIntelligenceTextAnimation()
{
    [webView() _didEndPartialIntelligenceTextAnimation];
}

bool PageClientImplCocoa::writingToolsTextReplacementsFinished()
{
    return [webView() _writingToolsTextReplacementsFinished];
}

void PageClientImplCocoa::addTextAnimationForAnimationID(const WTF::UUID& uuid, const WebCore::TextAnimationData& data)
{
    [webView() _addTextAnimationForAnimationID:uuid.createNSUUID().get() withData:data];
}

void PageClientImplCocoa::removeTextAnimationForAnimationID(const WTF::UUID& uuid)
{
    [webView() _removeTextAnimationForAnimationID:uuid.createNSUUID().get()];
}

#endif

#if ENABLE(SCREEN_TIME)
void PageClientImplCocoa::didChangeScreenTimeWebpageControllerURL()
{
    updateScreenTimeWebpageControllerURL(webView().get());
}

void PageClientImplCocoa::updateScreenTimeWebpageControllerURL(WKWebView *webView)
{
    if (!PAL::isScreenTimeFrameworkAvailable())
        return;

    RefPtr pageProxy = [webView _page].get();
    if (pageProxy && !pageProxy->preferences().screenTimeEnabled()) {
        [webView _uninstallScreenTimeWebpageController];
        return;
    }

    if ([webView window])
        [webView _installScreenTimeWebpageControllerIfNeeded];

    RetainPtr screenTimeWebpageController = [webView _screenTimeWebpageController];
    [screenTimeWebpageController setURL:[webView _mainFrameURL]];
}

void PageClientImplCocoa::setURLIsPictureInPictureForScreenTime(bool value)
{
    RetainPtr screenTimeWebpageController = [webView() _screenTimeWebpageController];
    if (!screenTimeWebpageController)
        return;

    [screenTimeWebpageController setURLIsPictureInPicture:value];
}

void PageClientImplCocoa::setURLIsPlayingVideoForScreenTime(bool value)
{
    RetainPtr screenTimeWebpageController = [webView() _screenTimeWebpageController];
    if (!screenTimeWebpageController)
        return;

    [screenTimeWebpageController setURLIsPlayingVideo:value];
}

#endif

void PageClientImplCocoa::viewIsBecomingVisible()
{
#if ENABLE(SCREEN_TIME)
    [m_webView _updateScreenTimeBasedOnWindowVisibility];
#endif
}

void PageClientImplCocoa::viewIsBecomingInvisible()
{
#if ENABLE(SCREEN_TIME)
    [m_webView _updateScreenTimeBasedOnWindowVisibility];
#endif
}

#if ENABLE(GAMEPAD)
void PageClientImplCocoa::setGamepadsRecentlyAccessed(GamepadsRecentlyAccessed gamepadsRecentlyAccessed)
{
    [webView() _setGamepadsRecentlyAccessed:(gamepadsRecentlyAccessed == GamepadsRecentlyAccessed::No) ? NO : YES];
}

#if PLATFORM(VISION)
void PageClientImplCocoa::gamepadsConnectedStateChanged()
{
    [m_webView _gamepadsConnectedStateChanged];
}
#endif
#endif

void PageClientImplCocoa::hasActiveNowPlayingSessionChanged(bool hasActiveNowPlayingSession)
{
    RetainPtr webView = m_webView.get();
    if ([webView _hasActiveNowPlayingSession] == hasActiveNowPlayingSession)
        return;

    RELEASE_LOG(ViewState, "%p PageClientImplCocoa::hasActiveNowPlayingSessionChanged %d", webView.get(), hasActiveNowPlayingSession);

    [webView willChangeValueForKey:@"_hasActiveNowPlayingSession"];
    [webView _setHasActiveNowPlayingSession:hasActiveNowPlayingSession];
    [webView didChangeValueForKey:@"_hasActiveNowPlayingSession"];
}

void PageClientImplCocoa::videoControlsManagerDidChange()
{
    RetainPtr webView = m_webView.get();
    RELEASE_LOG(ViewState, "%p PageClientImplCocoa::videoControlsManagerDidChange %d", webView.get(), [webView _canEnterFullscreen]);
    [webView willChangeValueForKey:@"_canEnterFullscreen"];
    [webView didChangeValueForKey:@"_canEnterFullscreen"];
}

CocoaWindow *PageClientImplCocoa::platformWindow() const
{
    return [webView() window];
}

void PageClientImplCocoa::processDidUpdateThrottleState()
{
    RetainPtr webView = m_webView.get();
    [webView willChangeValueForKey:@"_webProcessState"];
    [webView didChangeValueForKey:@"_webProcessState"];
}

#if ENABLE(FULLSCREEN_API)
void PageClientImplCocoa::setFullScreenClientForTesting(std::unique_ptr<WebFullScreenManagerProxyClient>&& client)
{
    m_fullscreenClientForTesting = WTFMove(client);
}
#endif

void PageClientImplCocoa::didCommitLayerTree(const RemoteLayerTreeTransaction& transaction, const std::optional<MainFrameData>&, const PageData&, const TransactionID&)
{
    [webView() _updateScrollGeometryWithContentOffset:transaction.scrollPosition() contentSize:transaction.scrollGeometryContentSize()];
}

void PageClientImplCocoa::didCommitMainFrameData(const MainFrameData& mainFrameData)
{
    if (mainFrameData.fixedContainerEdges)
        [webView() _updateFixedContainerEdges:*mainFrameData.fixedContainerEdges];
}

} // namespace WebKit
