/*
 * Copyright (C) 2012-2024 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "APINavigationAction.h"
#import "APIPageConfiguration.h"
#import "APIUIClient.h"
#import "APIWebsitePolicies.h"
#import "Connection.h"
#import "DocumentEditingContext.h"
#import "DrawingAreaProxy.h"
#import "EditingRange.h"
#import "GlobalFindInPageState.h"
#import "InteractionInformationAtPosition.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "NativeWebKeyboardEvent.h"
#import "NavigationState.h"
#import "PageClient.h"
#import "PaymentAuthorizationController.h"
#import "PrintInfo.h"
#import "ProvisionalPageProxy.h"
#import "RemoteLayerTreeHost.h"
#import "RemoteLayerTreeNode.h"
#import "RemoteLayerTreeTransaction.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "RevealItem.h"
#import "TapHandlingResult.h"
#import "UIKitSPI.h"
#import "UserData.h"
#import "VideoPresentationManagerProxy.h"
#import "ViewUpdateDispatcherMessages.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WebAutocorrectionContext.h"
#import "WebAutocorrectionData.h"
#import "WebCoreArgumentCoders.h"
#import "WebPage.h"
#import "WebPageMessages.h"
#import "WebPageProxyInternals.h"
#import "WebProcessMessages.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import "WebScreenOrientationManagerProxy.h"
#import <WebCore/AGXCompilerService.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/Quirks.h>
#import <WebCore/ShareableResource.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/UserAgent.h>
#import <WebCore/ValidationBubble.h>
#import <pal/spi/ios/MobileGestaltSPI.h>
#import <pal/system/ios/UserInterfaceIdiom.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/text/TextStream.h>

#if USE(QUICK_LOOK)
#import "APILoaderClient.h"
#import "APINavigationClient.h"
#import <wtf/text/WTFString.h>
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, legacyMainFrameProcess().connection())

#define WEBPAGEPROXY_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [pageProxyID=%llu, webPageID=%llu, PID=%i] WebPageProxy::" fmt, this, identifier().toUInt64(), webPageIDInMainFrameProcess().toUInt64(), m_legacyMainFrameProcess->processID(), ##__VA_ARGS__)

#if PLATFORM(VISION)
static constexpr CGFloat kTargetFullscreenAspectRatio = 1.7778;
#endif

namespace WebKit {
using namespace WebCore;

void WebPageProxy::platformInitialize()
{
}

PlatformDisplayID WebPageProxy::generateDisplayIDFromPageID() const
{
    // In order to ensure that we get a unique DisplayRefreshMonitor per-DrawingArea (necessary because DisplayRefreshMonitor
    // is driven by that class), give each page a unique DisplayID derived from WebPage's unique ID.
    // FIXME: While using the high end of the range of DisplayIDs makes a collision with real, non-RemoteLayerTreeDrawingArea
    // DisplayIDs less likely, it is not entirely safe to have a RemoteLayerTreeDrawingArea and TiledCoreAnimationDrawingArea
    // coeexist in the same process.
    return std::numeric_limits<uint32_t>::max() - webPageIDInMainFrameProcess().toUInt64();
}

String WebPageProxy::userAgentForURL(const URL&)
{
    return userAgent();
}

String WebPageProxy::standardUserAgent(const String& applicationNameForUserAgent)
{
    return standardUserAgentWithApplicationName(applicationNameForUserAgent);
}

void WebPageProxy::getIsSpeaking(CompletionHandler<void(bool)>&& completionHandler)
{
    notImplemented();
    completionHandler(false);
}

void WebPageProxy::speak(const String&)
{
    notImplemented();
}

void WebPageProxy::stopSpeaking()
{
    notImplemented();
}

void WebPageProxy::searchTheWeb(const String&)
{
    notImplemented();
}

void WebPageProxy::windowAndViewFramesChanged(const FloatRect&, const FloatPoint&)
{
    notImplemented();
}

String WebPageProxy::stringSelectionForPasteboard()
{
    notImplemented();
    return String();
}

RefPtr<WebCore::SharedBuffer> WebPageProxy::dataSelectionForPasteboard(const String&)
{
    notImplemented();
    return nullptr;
}

bool WebPageProxy::readSelectionFromPasteboard(const String&)
{
    notImplemented();
    return false;
}

void WebPageProxy::requestFocusedElementInformation(CompletionHandler<void(const std::optional<FocusedElementInformation>&)>&& callback)
{
    if (!hasRunningProcess())
        return callback({ });

    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::RequestFocusedElementInformation(), WTFMove(callback), webPageIDInMainFrameProcess());
}

void WebPageProxy::updateVisibleContentRects(const VisibleContentRectUpdateInfo& visibleContentRectUpdate, bool sendEvenIfUnchanged)
{
    if (visibleContentRectUpdate == internals().lastVisibleContentRectUpdate && !sendEvenIfUnchanged)
        return;

    internals().lastVisibleContentRectUpdate = visibleContentRectUpdate;

    if (!hasRunningProcess())
        return;

    m_legacyMainFrameProcess->send(Messages::ViewUpdateDispatcher::VisibleContentRectUpdate(webPageIDInMainFrameProcess(), visibleContentRectUpdate), 0);
}

void WebPageProxy::resendLastVisibleContentRects()
{
    m_legacyMainFrameProcess->send(Messages::ViewUpdateDispatcher::VisibleContentRectUpdate(webPageIDInMainFrameProcess(), internals().lastVisibleContentRectUpdate), 0);
}

void WebPageProxy::updateStringForFind(const String& string)
{
    if (!hasRunningProcess())
        return;

    WebKit::updateStringForFind(string);
}

static inline float adjustedUnexposedEdge(float documentEdge, float exposedRectEdge, float factor)
{
    if (exposedRectEdge < documentEdge)
        return documentEdge - factor * (documentEdge - exposedRectEdge);
    
    return exposedRectEdge;
}

static inline float adjustedUnexposedMaxEdge(float documentEdge, float exposedRectEdge, float factor)
{
    if (exposedRectEdge > documentEdge)
        return documentEdge + factor * (exposedRectEdge - documentEdge);
    
    return exposedRectEdge;
}

WebCore::FloatRect WebPageProxy::computeLayoutViewportRect(const FloatRect& unobscuredContentRect, const FloatRect& unobscuredContentRectRespectingInputViewBounds, const FloatRect& currentLayoutViewportRect, double displayedContentScale, LayoutViewportConstraint constraint) const
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return { };

    FloatRect constrainedUnobscuredRect = unobscuredContentRect;
    FloatRect documentRect = pageClient->documentRect();

    if (constraint == LayoutViewportConstraint::ConstrainedToDocumentRect)
        constrainedUnobscuredRect.intersect(documentRect);

    double minimumScale = pageClient->minimumZoomScale();
    bool isBelowMinimumScale = displayedContentScale < minimumScale && !WebKit::scalesAreEssentiallyEqual(displayedContentScale, minimumScale);
    if (isBelowMinimumScale) {
        const CGFloat slope = 12;
        CGFloat factor = std::max<CGFloat>(1 - slope * (minimumScale - displayedContentScale), 0);

        constrainedUnobscuredRect.setX(adjustedUnexposedEdge(documentRect.x(), constrainedUnobscuredRect.x(), factor));
        constrainedUnobscuredRect.setY(adjustedUnexposedEdge(documentRect.y(), constrainedUnobscuredRect.y(), factor));
        constrainedUnobscuredRect.setWidth(adjustedUnexposedMaxEdge(documentRect.maxX(), constrainedUnobscuredRect.maxX(), factor) - constrainedUnobscuredRect.x());
        constrainedUnobscuredRect.setHeight(adjustedUnexposedMaxEdge(documentRect.maxY(), constrainedUnobscuredRect.maxY(), factor) - constrainedUnobscuredRect.y());
    }

    FloatSize constrainedSize = isBelowMinimumScale ? constrainedUnobscuredRect.size() : unobscuredContentRect.size();
    FloatRect unobscuredContentRectForViewport = isBelowMinimumScale ? constrainedUnobscuredRect : unobscuredContentRectRespectingInputViewBounds;

    double heightExpansionFactor = internals().allowsLayoutViewportHeightExpansion ? m_preferences->layoutViewportHeightExpansionFactor() : 0;
    auto layoutViewportSize = LocalFrameView::expandedLayoutViewportSize(internals().baseLayoutViewportSize, LayoutSize(documentRect.size()), heightExpansionFactor);
    FloatRect layoutViewportRect = LocalFrameView::computeUpdatedLayoutViewportRect(LayoutRect(currentLayoutViewportRect), LayoutRect(documentRect), LayoutSize(constrainedSize), LayoutRect(unobscuredContentRectForViewport), layoutViewportSize, internals().minStableLayoutViewportOrigin, internals().maxStableLayoutViewportOrigin, constraint);
    
    if (layoutViewportRect != currentLayoutViewportRect)
        LOG_WITH_STREAM(VisibleRects, stream << "WebPageProxy::computeLayoutViewportRect: new layout viewport  " << layoutViewportRect);
    return layoutViewportRect;
}

FloatRect WebPageProxy::unconstrainedLayoutViewportRect() const
{
    return computeLayoutViewportRect(unobscuredContentRect(), unobscuredContentRectRespectingInputViewBounds(), layoutViewportRect(), displayedContentScale(), LayoutViewportConstraint::Unconstrained);
}

void WebPageProxy::scrollingNodeScrollViewWillStartPanGesture(ScrollingNodeID nodeID)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->scrollingNodeScrollViewWillStartPanGesture(nodeID);
}

void WebPageProxy::scrollingNodeScrollWillStartScroll(std::optional<ScrollingNodeID> nodeID)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->scrollingNodeScrollWillStartScroll(nodeID);
}

void WebPageProxy::scrollingNodeScrollDidEndScroll(std::optional<ScrollingNodeID> nodeID)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->scrollingNodeScrollDidEndScroll(nodeID);
}

void WebPageProxy::dynamicViewportSizeUpdate(const DynamicViewportSizeUpdate& target)
{
    if (!hasRunningProcess())
        return;
    hideValidationMessage();
    internals().viewportConfigurationViewLayoutSize = target.viewLayoutSize;
    internals().defaultUnobscuredSize = target.maximumUnobscuredSize;
    internals().minimumUnobscuredSize = target.minimumUnobscuredSize;
    internals().maximumUnobscuredSize = target.maximumUnobscuredSize;
    m_legacyMainFrameProcess->send(Messages::WebPage::DynamicViewportSizeUpdate(target), webPageIDInMainFrameProcess());
    setDeviceOrientation(target.deviceOrientation);
}

void WebPageProxy::setViewportConfigurationViewLayoutSize(const WebCore::FloatSize& size, double layoutSizeScaleFactorFromClient, double minimumEffectiveDeviceWidth)
{
    internals().viewportConfigurationViewLayoutSize = size;
    m_viewportConfigurationLayoutSizeScaleFactorFromClient = layoutSizeScaleFactorFromClient;
    m_viewportConfigurationMinimumEffectiveDeviceWidth = minimumEffectiveDeviceWidth;

    if (m_provisionalPage)
        m_provisionalPage->send(Messages::WebPage::SetViewportConfigurationViewLayoutSize(size, layoutSizeScaleFactorFromClient, minimumEffectiveDeviceWidth));
    if (hasRunningProcess())
        m_legacyMainFrameProcess->send(Messages::WebPage::SetViewportConfigurationViewLayoutSize(size, layoutSizeScaleFactorFromClient, minimumEffectiveDeviceWidth), webPageIDInMainFrameProcess());
}

void WebPageProxy::setSceneIdentifier(String&& sceneIdentifier)
{
    if (hasRunningProcess())
        m_legacyMainFrameProcess->send(Messages::WebPage::SetSceneIdentifier(WTFMove(sceneIdentifier)), webPageIDInMainFrameProcess());
}

void WebPageProxy::setForceAlwaysUserScalable(bool userScalable)
{
    if (m_forceAlwaysUserScalable == userScalable)
        return;
    m_forceAlwaysUserScalable = userScalable;

    if (hasRunningProcess())
        m_legacyMainFrameProcess->send(Messages::WebPage::SetForceAlwaysUserScalable(userScalable), webPageIDInMainFrameProcess());
}

WebCore::ScreenOrientationType WebPageProxy::toScreenOrientationType(IntDegrees angle)
{
    if (angle == -90)
        return WebCore::ScreenOrientationType::LandscapeSecondary;
    if (angle == 180)
        return WebCore::ScreenOrientationType::PortraitSecondary;
    if (angle == 90)
        return WebCore::ScreenOrientationType::LandscapePrimary;
    return WebCore::ScreenOrientationType::PortraitPrimary;
}

void WebPageProxy::setDeviceOrientation(IntDegrees deviceOrientation)
{
    if (m_screenOrientationManager)
        m_screenOrientationManager->setCurrentOrientation(toScreenOrientationType(deviceOrientation));

    if (deviceOrientation != m_deviceOrientation) {
        m_deviceOrientation = deviceOrientation;
        if (hasRunningProcess())
            m_legacyMainFrameProcess->send(Messages::WebPage::SetDeviceOrientation(deviceOrientation), webPageIDInMainFrameProcess());
    }
}

void WebPageProxy::setOverrideViewportArguments(const std::optional<ViewportArguments>& viewportArguments)
{
    if (viewportArguments == internals().overrideViewportArguments)
        return;
    internals().overrideViewportArguments = viewportArguments;
    if (hasRunningProcess())
        m_legacyMainFrameProcess->send(Messages::WebPage::SetOverrideViewportArguments(viewportArguments), webPageIDInMainFrameProcess());
}

bool WebPageProxy::updateLayoutViewportParameters(const RemoteLayerTreeTransaction& layerTreeTransaction)
{
    if (internals().baseLayoutViewportSize == layerTreeTransaction.baseLayoutViewportSize()
        && internals().minStableLayoutViewportOrigin == layerTreeTransaction.minStableLayoutViewportOrigin()
        && internals().maxStableLayoutViewportOrigin == layerTreeTransaction.maxStableLayoutViewportOrigin())
        return false;
    internals().baseLayoutViewportSize = layerTreeTransaction.baseLayoutViewportSize();
    internals().minStableLayoutViewportOrigin = layerTreeTransaction.minStableLayoutViewportOrigin();
    internals().maxStableLayoutViewportOrigin = layerTreeTransaction.maxStableLayoutViewportOrigin();
    LOG_WITH_STREAM(VisibleRects, stream << "WebPageProxy::updateLayoutViewportParameters: baseLayoutViewportSize: " << internals().baseLayoutViewportSize << " minStableLayoutViewportOrigin: " << internals().minStableLayoutViewportOrigin << " maxStableLayoutViewportOrigin: " << internals().maxStableLayoutViewportOrigin);
    return true;
}

void WebPageProxy::selectWithGesture(IntPoint point, GestureType gestureType, GestureRecognizerState gestureState, bool isInteractingWithFocusedElement, CompletionHandler<void(const IntPoint&, GestureType, GestureRecognizerState, OptionSet<SelectionFlags>)>&& callback)
{
    if (!hasRunningProcess())
        return callback({ }, GestureType::Loupe, GestureRecognizerState::Possible, { });

    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::SelectWithGesture(point, gestureType, gestureState, isInteractingWithFocusedElement), WTFMove(callback), webPageIDInMainFrameProcess());
}

void WebPageProxy::updateSelectionWithTouches(IntPoint point, SelectionTouch touches, bool baseIsStart, CompletionHandler<void(const WebCore::IntPoint&, SelectionTouch, OptionSet<SelectionFlags>)>&& callback)
{
    if (!hasRunningProcess())
        return callback(WebCore::IntPoint(), SelectionTouch::Started, { });

    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::UpdateSelectionWithTouches(point, touches, baseIsStart), WTFMove(callback), webPageIDInMainFrameProcess());
}

void WebPageProxy::willInsertFinalDictationResult()
{
    m_legacyMainFrameProcess->send(Messages::WebPage::WillInsertFinalDictationResult(), webPageIDInMainFrameProcess());
}

void WebPageProxy::didInsertFinalDictationResult()
{
    m_legacyMainFrameProcess->send(Messages::WebPage::DidInsertFinalDictationResult(), webPageIDInMainFrameProcess());
}

void WebPageProxy::replaceDictatedText(const String& oldText, const String& newText)
{
    m_legacyMainFrameProcess->send(Messages::WebPage::ReplaceDictatedText(oldText, newText), webPageIDInMainFrameProcess());
}

void WebPageProxy::replaceSelectedText(const String& oldText, const String& newText)
{
    m_legacyMainFrameProcess->send(Messages::WebPage::ReplaceSelectedText(oldText, newText), webPageIDInMainFrameProcess());
}

void WebPageProxy::requestAutocorrectionData(const String& textForAutocorrection, CompletionHandler<void(WebAutocorrectionData)>&& callback)
{
    if (!hasRunningProcess()) {
        callback({ });
        return;
    }
    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::RequestAutocorrectionData(textForAutocorrection), WTFMove(callback), webPageIDInMainFrameProcess());
}

void WebPageProxy::applyAutocorrection(const String& correction, const String& originalText, bool isCandidate, CompletionHandler<void(const String&)>&& callback)
{
    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::ApplyAutocorrection(correction, originalText, isCandidate), WTFMove(callback), webPageIDInMainFrameProcess());
}

bool WebPageProxy::applyAutocorrection(const String& correction, const String& originalText, bool isCandidate)
{
    auto sendSync = m_legacyMainFrameProcess->sendSync(Messages::WebPage::SyncApplyAutocorrection(correction, originalText, isCandidate), webPageIDInMainFrameProcess());
    auto [autocorrectionApplied] = sendSync.takeReplyOr(false);
    return autocorrectionApplied;
}

void WebPageProxy::selectTextWithGranularityAtPoint(const WebCore::IntPoint point, WebCore::TextGranularity granularity, bool isInteractingWithFocusedElement, CompletionHandler<void()>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction();
        return;
    }

    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::SelectTextWithGranularityAtPoint(point, granularity, isInteractingWithFocusedElement), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = m_legacyMainFrameProcess->throttler().backgroundActivity("WebPageProxy::selectTextWithGranularityAtPoint"_s)] () mutable {
        callbackFunction();
    }, webPageIDInMainFrameProcess());
}

void WebPageProxy::selectPositionAtBoundaryWithDirection(const WebCore::IntPoint point, WebCore::TextGranularity granularity, WebCore::SelectionDirection direction, bool isInteractingWithFocusedElement, CompletionHandler<void()>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction();
        return;
    }

    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::SelectPositionAtBoundaryWithDirection(point, granularity, direction, isInteractingWithFocusedElement), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = m_legacyMainFrameProcess->throttler().backgroundActivity("WebPageProxy::selectPositionAtBoundaryWithDirection"_s)] () mutable {
        callbackFunction();
    }, webPageIDInMainFrameProcess());
}

void WebPageProxy::moveSelectionAtBoundaryWithDirection(WebCore::TextGranularity granularity, WebCore::SelectionDirection direction, CompletionHandler<void()>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction();
        return;
    }

    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::MoveSelectionAtBoundaryWithDirection(granularity, direction), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = m_legacyMainFrameProcess->throttler().backgroundActivity("WebPageProxy::moveSelectionAtBoundaryWithDirection"_s)] () mutable {
        callbackFunction();
    }, webPageIDInMainFrameProcess());
}

void WebPageProxy::selectPositionAtPoint(const WebCore::IntPoint point, bool isInteractingWithFocusedElement, CompletionHandler<void()>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction();
        return;
    }

    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::SelectPositionAtPoint(point, isInteractingWithFocusedElement), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = m_legacyMainFrameProcess->throttler().backgroundActivity("WebPageProxy::selectPositionAtPoint"_s)] () mutable {
        callbackFunction();
    }, webPageIDInMainFrameProcess());
}

void WebPageProxy::beginSelectionInDirection(WebCore::SelectionDirection direction, CompletionHandler<void(bool)>&& callback)
{
    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::BeginSelectionInDirection(direction), WTFMove(callback), webPageIDInMainFrameProcess());
}

void WebPageProxy::updateSelectionWithExtentPoint(const WebCore::IntPoint point, bool isInteractingWithFocusedElement, RespectSelectionAnchor respectSelectionAnchor, CompletionHandler<void(bool)>&& callback)
{
    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::UpdateSelectionWithExtentPoint(point, isInteractingWithFocusedElement, respectSelectionAnchor), WTFMove(callback), webPageIDInMainFrameProcess());
}

void WebPageProxy::updateSelectionWithExtentPointAndBoundary(const WebCore::IntPoint point, WebCore::TextGranularity granularity, bool isInteractingWithFocusedElement, CompletionHandler<void(bool)>&& callback)
{
    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::UpdateSelectionWithExtentPointAndBoundary(point, granularity, isInteractingWithFocusedElement), WTFMove(callback), webPageIDInMainFrameProcess());
}

#if ENABLE(REVEAL)
void WebPageProxy::requestRVItemInCurrentSelectedRange(CompletionHandler<void(const WebKit::RevealItem&)>&& callbackFunction)
{
    if (!hasRunningProcess())
        return callbackFunction(RevealItem());

    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::RequestRVItemInCurrentSelectedRange(), WTFMove(callbackFunction), webPageIDInMainFrameProcess());
}

void WebPageProxy::prepareSelectionForContextMenuWithLocationInView(IntPoint point, CompletionHandler<void(bool, const RevealItem&)>&& callbackFunction)
{
    if (!hasRunningProcess())
        return callbackFunction(false, RevealItem());

    dispatchAfterCurrentContextMenuEvent([weakThis = WeakPtr { *this }, point, callbackFunction = WTFMove(callbackFunction)] (bool handled) mutable {
        if (!weakThis || handled) {
            callbackFunction(false, RevealItem());
            return;
        }

        weakThis->legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::PrepareSelectionForContextMenuWithLocationInView(point), WTFMove(callbackFunction), weakThis->webPageIDInMainFrameProcess());
    });
}
#endif

void WebPageProxy::requestAutocorrectionContext()
{
    m_legacyMainFrameProcess->send(Messages::WebPage::HandleAutocorrectionContextRequest(), webPageIDInMainFrameProcess());
}

void WebPageProxy::handleAutocorrectionContext(const WebAutocorrectionContext& context)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->handleAutocorrectionContext(context);
}

void WebPageProxy::clearSelectionAfterTappingSelectionHighlightIfNeeded(WebCore::FloatPoint location)
{
    if (hasRunningProcess())
        legacyMainFrameProcess().send(Messages::WebPage::ClearSelectionAfterTappingSelectionHighlightIfNeeded(location), webPageIDInMainFrameProcess());
}

void WebPageProxy::getSelectionContext(CompletionHandler<void(const String&, const String&, const String&)>&& callbackFunction)
{
    if (!hasRunningProcess())
        return callbackFunction({ }, { }, { });
    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::GetSelectionContext(), WTFMove(callbackFunction), webPageIDInMainFrameProcess());
}

void WebPageProxy::handleTwoFingerTapAtPoint(const WebCore::IntPoint& point, OptionSet<WebEventModifier> modifiers, WebKit::TapIdentifier requestID)
{
    legacyMainFrameProcess().send(Messages::WebPage::HandleTwoFingerTapAtPoint(point, modifiers, requestID), webPageIDInMainFrameProcess());
}

void WebPageProxy::selectWithTwoTouches(const WebCore::IntPoint from, const WebCore::IntPoint to, GestureType gestureType, GestureRecognizerState gestureState, CompletionHandler<void(const WebCore::IntPoint&, GestureType, GestureRecognizerState, OptionSet<SelectionFlags>)>&& callback)
{
    if (!hasRunningProcess())
        return callback({ }, GestureType::Loupe, GestureRecognizerState::Possible, { });

    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::SelectWithTwoTouches(from, to, gestureType, gestureState), WTFMove(callback), webPageIDInMainFrameProcess());
}

void WebPageProxy::didReceivePositionInformation(const InteractionInformationAtPosition& info)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->positionInformationDidChange(info);
}

void WebPageProxy::requestPositionInformation(const InteractionInformationRequest& request)
{
    m_legacyMainFrameProcess->send(Messages::WebPage::RequestPositionInformation(request), webPageIDInMainFrameProcess());
}

void WebPageProxy::startInteractionWithPositionInformation(const InteractionInformationAtPosition& positionInformation)
{
    m_legacyMainFrameProcess->send(Messages::WebPage::StartInteractionWithElementContextOrPosition(positionInformation.elementContext, positionInformation.request.point), webPageIDInMainFrameProcess());
}

void WebPageProxy::stopInteraction()
{
    m_legacyMainFrameProcess->send(Messages::WebPage::StopInteraction(), webPageIDInMainFrameProcess());
}

bool WebPageProxy::isValidPerformActionOnElementAuthorizationToken(const String& authorizationToken) const
{
    return !authorizationToken.isNull() && m_performActionOnElementAuthTokens.contains(authorizationToken);
}

void WebPageProxy::performActionOnElement(uint32_t action)
{
    auto authorizationToken = createVersion4UUIDString();

    m_performActionOnElementAuthTokens.add(authorizationToken);
    
    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::PerformActionOnElement(action, authorizationToken), [weakThis = WeakPtr { *this }, authorizationToken] () mutable {
        if (!weakThis)
            return;

        ASSERT(weakThis->isValidPerformActionOnElementAuthorizationToken(authorizationToken));
        weakThis->m_performActionOnElementAuthTokens.remove(authorizationToken);
    }, webPageIDInMainFrameProcess());
}

void WebPageProxy::performActionOnElements(uint32_t action, Vector<WebCore::ElementContext>&& elements)
{
    m_legacyMainFrameProcess->send(Messages::WebPage::PerformActionOnElements(action, elements), webPageIDInMainFrameProcess());
}

void WebPageProxy::saveImageToLibrary(SharedMemory::Handle&& imageHandle, const String& authorizationToken)
{
    MESSAGE_CHECK(isValidPerformActionOnElementAuthorizationToken(authorizationToken));

    auto sharedMemoryBuffer = SharedMemory::map(WTFMove(imageHandle), SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryBuffer)
        return;

    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    auto buffer = sharedMemoryBuffer->createSharedBuffer(sharedMemoryBuffer->size());
    pageClient->saveImageToLibrary(WTFMove(buffer));
}

void WebPageProxy::applicationDidEnterBackground()
{
    m_lastObservedStateWasBackground = true;

    bool isSuspendedUnderLock = UIApplication.sharedApplication.isSuspendedUnderLock;
    
    WEBPAGEPROXY_RELEASE_LOG(ViewState, "applicationDidEnterBackground: isSuspendedUnderLock? %d", isSuspendedUnderLock);

#if !PLATFORM(WATCHOS)
    // We normally delay process suspension when the app is backgrounded until the current page load completes. However,
    // we do not want to do so when the screen is locked for power reasons.
    if (isSuspendedUnderLock) {
        if (auto* navigationState = NavigationState::fromWebPage(*this))
            navigationState->releaseNetworkActivity(NavigationState::NetworkActivityReleaseReason::ScreenLocked);
    }
#endif
    m_legacyMainFrameProcess->send(Messages::WebPage::ApplicationDidEnterBackground(isSuspendedUnderLock), webPageIDInMainFrameProcess());
}

void WebPageProxy::applicationDidFinishSnapshottingAfterEnteringBackground()
{
    if (m_drawingArea) {
        m_drawingArea->prepareForAppSuspension();
        m_drawingArea->hideContentUntilPendingUpdate();
    }
    m_legacyMainFrameProcess->send(Messages::WebPage::ApplicationDidFinishSnapshottingAfterEnteringBackground(), webPageIDInMainFrameProcess());
}

void WebPageProxy::applicationWillEnterForeground()
{
    m_lastObservedStateWasBackground = false;

    bool isSuspendedUnderLock = UIApplication.sharedApplication.isSuspendedUnderLock;
    WEBPAGEPROXY_RELEASE_LOG(ViewState, "applicationWillEnterForeground: isSuspendedUnderLock? %d", isSuspendedUnderLock);

    m_legacyMainFrameProcess->send(Messages::WebPage::ApplicationWillEnterForeground(isSuspendedUnderLock), webPageIDInMainFrameProcess());

    hardwareKeyboardAvailabilityChanged(m_legacyMainFrameProcess->processPool().cachedHardwareKeyboardState());
}

void WebPageProxy::applicationWillResignActive()
{
    m_legacyMainFrameProcess->send(Messages::WebPage::ApplicationWillResignActive(), webPageIDInMainFrameProcess());
}

void WebPageProxy::applicationDidEnterBackgroundForMedia()
{
    bool isSuspendedUnderLock = UIApplication.sharedApplication.isSuspendedUnderLock;
    WEBPAGEPROXY_RELEASE_LOG(ViewState, "applicationWillEnterForegroundForMedia: isSuspendedUnderLock? %d", isSuspendedUnderLock);

    m_legacyMainFrameProcess->send(Messages::WebPage::ApplicationDidEnterBackgroundForMedia(isSuspendedUnderLock), webPageIDInMainFrameProcess());
}

void WebPageProxy::applicationWillEnterForegroundForMedia()
{
    bool isSuspendedUnderLock = UIApplication.sharedApplication.isSuspendedUnderLock;
    WEBPAGEPROXY_RELEASE_LOG(ViewState, "applicationWillEnterForegroundForMedia: isSuspendedUnderLock? %d", isSuspendedUnderLock);

    m_legacyMainFrameProcess->send(Messages::WebPage::ApplicationWillEnterForegroundForMedia(isSuspendedUnderLock), webPageIDInMainFrameProcess());
}

void WebPageProxy::applicationDidBecomeActive()
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (m_videoPresentationManager)
        m_videoPresentationManager->applicationDidBecomeActive();
#endif
    m_legacyMainFrameProcess->send(Messages::WebPage::ApplicationDidBecomeActive(), webPageIDInMainFrameProcess());
}

void WebPageProxy::extendSelection(WebCore::TextGranularity granularity, CompletionHandler<void()>&& completionHandler)
{
    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::ExtendSelection(granularity), [completionHandler = WTFMove(completionHandler)]() mutable {
        if (completionHandler)
            completionHandler();
    }, webPageIDInMainFrameProcess());
}

void WebPageProxy::selectWordBackward()
{
    m_legacyMainFrameProcess->send(Messages::WebPage::SelectWordBackward(), webPageIDInMainFrameProcess());
}

void WebPageProxy::extendSelectionForReplacement(CompletionHandler<void()>&& completion)
{
    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::ExtendSelectionForReplacement(), WTFMove(completion), webPageIDInMainFrameProcess());
}

void WebPageProxy::requestRectsForGranularityWithSelectionOffset(WebCore::TextGranularity granularity, uint32_t offset, CompletionHandler<void(const Vector<WebCore::SelectionGeometry>&)>&& callback)
{
    if (!hasRunningProcess())
        return callback({ });
    
    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::GetRectsForGranularityWithSelectionOffset(granularity, offset), WTFMove(callback), webPageIDInMainFrameProcess());
}

void WebPageProxy::requestRectsAtSelectionOffsetWithText(int32_t offset, const String& text, CompletionHandler<void(const Vector<WebCore::SelectionGeometry>&)>&& callback)
{
    if (!hasRunningProcess())
        return callback({ });
    
    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::GetRectsAtSelectionOffsetWithText(offset, text), WTFMove(callback), webPageIDInMainFrameProcess());
}

void WebPageProxy::storeSelectionForAccessibility(bool shouldStore)
{
    m_legacyMainFrameProcess->send(Messages::WebPage::StoreSelectionForAccessibility(shouldStore), webPageIDInMainFrameProcess());
}

void WebPageProxy::startAutoscrollAtPosition(const WebCore::FloatPoint& positionInWindow)
{
    m_legacyMainFrameProcess->send(Messages::WebPage::StartAutoscrollAtPosition(positionInWindow), webPageIDInMainFrameProcess());
}

void WebPageProxy::cancelAutoscroll()
{
    m_legacyMainFrameProcess->send(Messages::WebPage::CancelAutoscroll(), webPageIDInMainFrameProcess());
}

void WebPageProxy::moveSelectionByOffset(int32_t offset, CompletionHandler<void()>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction();
        return;
    }
    
    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::MoveSelectionByOffset(offset), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = m_legacyMainFrameProcess->throttler().backgroundActivity("WebPageProxy::moveSelectionByOffset"_s)] () mutable {
        callbackFunction();
    }, webPageIDInMainFrameProcess());
}

void WebPageProxy::interpretKeyEvent(const EditorState& state, bool isCharEvent, CompletionHandler<void(bool)>&& completionHandler)
{
    updateEditorState(state);
    if (!hasQueuedKeyEvent())
        return completionHandler(false);
    RefPtr pageClient = this->pageClient();
    completionHandler(pageClient && pageClient->interpretKeyEvent(firstQueuedKeyEvent(), isCharEvent));
}

void WebPageProxy::setSmartInsertDeleteEnabled(bool)
{
    notImplemented();
}

void WebPageProxy::registerWebProcessAccessibilityToken(std::span<const uint8_t> data)
{
    // Note: The WebFrameProxy with this FrameIdentifier might not exist in the UI process. See rdar://130998804.
    if (RefPtr pageClient = this->pageClient())
        pageClient->accessibilityWebProcessTokenReceived(data, protectedLegacyMainFrameProcess()->protectedConnection()->remoteProcessID());
}

void WebPageProxy::relayAccessibilityNotification(const String& notificationName, std::span<const uint8_t> data)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->relayAccessibilityNotification(notificationName, toNSData(data).get());
}

void WebPageProxy::assistiveTechnologyMakeFirstResponder()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->assistiveTechnologyMakeFirstResponder();
}

void WebPageProxy::makeFirstResponder()
{
    notImplemented();
}

void WebPageProxy::registerUIProcessAccessibilityTokens(std::span<const uint8_t> elementToken, std::span<const uint8_t> windowToken)
{
    if (!hasRunningProcess())
        return;

    legacyMainFrameProcess().send(Messages::WebPage::RegisterUIProcessAccessibilityTokens(elementToken, windowToken), webPageIDInMainFrameProcess());
}

void WebPageProxy::executeSavedCommandBySelector(IPC::Connection&, const String&, CompletionHandler<void(bool)>&& completionHandler)
{
    notImplemented();
    completionHandler(false);
}

bool WebPageProxy::shouldDelayWindowOrderingForEvent(const WebKit::WebMouseEvent&)
{
    notImplemented();
    return false;
}

void WebPageProxy::willStartUserTriggeredZooming()
{
    legacyMainFrameProcess().send(Messages::WebPage::WillStartUserTriggeredZooming(), webPageIDInMainFrameProcess());
}

void WebPageProxy::didEndUserTriggeredZooming()
{
    legacyMainFrameProcess().send(Messages::WebPage::DidEndUserTriggeredZooming(), webPageIDInMainFrameProcess());
}

void WebPageProxy::potentialTapAtPosition(const WebCore::FloatPoint& position, bool shouldRequestMagnificationInformation, WebKit::TapIdentifier requestID)
{
    hideValidationMessage();
    legacyMainFrameProcess().send(Messages::WebPage::PotentialTapAtPosition(requestID, position, shouldRequestMagnificationInformation), webPageIDInMainFrameProcess());
}

void WebPageProxy::commitPotentialTap(OptionSet<WebEventModifier> modifiers, TransactionID layerTreeTransactionIdAtLastTouchStart, WebCore::PointerID pointerId)
{
    legacyMainFrameProcess().send(Messages::WebPage::CommitPotentialTap(modifiers, layerTreeTransactionIdAtLastTouchStart, pointerId), webPageIDInMainFrameProcess());
}

void WebPageProxy::cancelPotentialTap()
{
    legacyMainFrameProcess().send(Messages::WebPage::CancelPotentialTap(), webPageIDInMainFrameProcess());
}

void WebPageProxy::tapHighlightAtPosition(const WebCore::FloatPoint& position, WebKit::TapIdentifier requestID)
{
    legacyMainFrameProcess().send(Messages::WebPage::TapHighlightAtPosition(requestID, position), webPageIDInMainFrameProcess());
}

void WebPageProxy::attemptSyntheticClick(const FloatPoint& location, OptionSet<WebEventModifier> modifiers, TransactionID layerTreeTransactionIdAtLastTouchStart)
{
    legacyMainFrameProcess().send(Messages::WebPage::AttemptSyntheticClick(roundedIntPoint(location), modifiers, layerTreeTransactionIdAtLastTouchStart), webPageIDInMainFrameProcess());
}

void WebPageProxy::didRecognizeLongPress()
{
    legacyMainFrameProcess().send(Messages::WebPage::DidRecognizeLongPress(), webPageIDInMainFrameProcess());
}

void WebPageProxy::handleDoubleTapForDoubleClickAtPoint(const WebCore::IntPoint& point, OptionSet<WebEventModifier> modifiers, TransactionID layerTreeTransactionIdAtLastTouchStart)
{
    legacyMainFrameProcess().send(Messages::WebPage::HandleDoubleTapForDoubleClickAtPoint(point, modifiers, layerTreeTransactionIdAtLastTouchStart), webPageIDInMainFrameProcess());
}

void WebPageProxy::inspectorNodeSearchMovedToPosition(const WebCore::FloatPoint& position)
{
    legacyMainFrameProcess().send(Messages::WebPage::InspectorNodeSearchMovedToPosition(position), webPageIDInMainFrameProcess());
}

void WebPageProxy::inspectorNodeSearchEndedAtPosition(const WebCore::FloatPoint& position)
{
    legacyMainFrameProcess().send(Messages::WebPage::InspectorNodeSearchEndedAtPosition(position), webPageIDInMainFrameProcess());
}

void WebPageProxy::blurFocusedElement()
{
    legacyMainFrameProcess().send(Messages::WebPage::BlurFocusedElement(), webPageIDInMainFrameProcess());
}

FloatSize WebPageProxy::screenSize()
{
    return WebCore::screenSize();
}

#if PLATFORM(VISION)
static FloatSize fullscreenPreferencesScreenSize(CGFloat preferredWidth)
{
    CGFloat preferredHeight = preferredWidth / kTargetFullscreenAspectRatio;
    return FloatSize(CGSizeMake(preferredWidth, preferredHeight));
}
#endif

FloatSize WebPageProxy::availableScreenSize()
{
#if PLATFORM(VISION)
    if (PAL::currentUserInterfaceIdiomIsVision())
        return fullscreenPreferencesScreenSize(m_preferences->mediaPreferredFullscreenWidth());
#endif
    return WebCore::availableScreenSize();
}

FloatSize WebPageProxy::overrideScreenSize()
{
#if PLATFORM(VISION)
    if (PAL::currentUserInterfaceIdiomIsVision())
        return fullscreenPreferencesScreenSize(m_preferences->mediaPreferredFullscreenWidth());
#endif
    return WebCore::overrideScreenSize();
}

FloatSize WebPageProxy::overrideAvailableScreenSize()
{
#if PLATFORM(VISION)
    if (PAL::currentUserInterfaceIdiomIsVision())
        return fullscreenPreferencesScreenSize(m_preferences->mediaPreferredFullscreenWidth());
#endif
    return WebCore::overrideAvailableScreenSize();
}

float WebPageProxy::textAutosizingWidth()
{
    return WebCore::screenSize().width();
}

void WebPageProxy::couldNotRestorePageState()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->couldNotRestorePageState();
}

void WebPageProxy::restorePageState(std::optional<WebCore::FloatPoint> scrollPosition, const WebCore::FloatPoint& scrollOrigin, const WebCore::FloatBoxExtent& obscuredInsetsOnSave, double scale)
{
    MESSAGE_CHECK(scale > 0);
    if (RefPtr pageClient = this->pageClient())
        pageClient->restorePageState(scrollPosition, scrollOrigin, obscuredInsetsOnSave, scale);
}

void WebPageProxy::restorePageCenterAndScale(std::optional<WebCore::FloatPoint> center, double scale)
{
    MESSAGE_CHECK(scale > 0);
    if (RefPtr pageClient = this->pageClient())
        pageClient->restorePageCenterAndScale(center, scale);
}

void WebPageProxy::didGetTapHighlightGeometries(WebKit::TapIdentifier requestID, const WebCore::Color& color, const Vector<WebCore::FloatQuad>& highlightedQuads, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius, bool nodeHasBuiltInClickHandling)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->didGetTapHighlightGeometries(requestID, color, highlightedQuads, topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius, nodeHasBuiltInClickHandling);
}

void WebPageProxy::setIsShowingInputViewForFocusedElement(bool showingInputView)
{
    legacyMainFrameProcess().send(Messages::WebPage::SetIsShowingInputViewForFocusedElement(showingInputView), webPageIDInMainFrameProcess());
}

void WebPageProxy::updateInputContextAfterBlurringAndRefocusingElement()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->updateInputContextAfterBlurringAndRefocusingElement();
}

void WebPageProxy::elementDidFocus(const FocusedElementInformation& information, bool userIsInteracting, bool blurPreviousNode, OptionSet<WebCore::ActivityState> activityStateChanges, const UserData& userData)
{
    m_pendingInputModeChange = std::nullopt;

    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    API::Object* userDataObject = legacyMainFrameProcess().transformHandlesToObjects(userData.object()).get();
    pageClient->elementDidFocus(information, userIsInteracting, blurPreviousNode, activityStateChanges, userDataObject);
}

void WebPageProxy::elementDidBlur()
{
    m_pendingInputModeChange = std::nullopt;
    if (RefPtr pageClient = this->pageClient())
        pageClient->elementDidBlur();
}

void WebPageProxy::updateFocusedElementInformation(const FocusedElementInformation& information)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->updateFocusedElementInformation(information);
}

void WebPageProxy::focusedElementDidChangeInputMode(WebCore::InputMode mode)
{
#if ENABLE(TOUCH_EVENTS)
    if (internals().touchEventTracking.isTrackingAnything()) {
        m_pendingInputModeChange = mode;
        return;
    }
#endif

    if (RefPtr pageClient = this->pageClient())
        pageClient->focusedElementDidChangeInputMode(mode);
}

void WebPageProxy::didReleaseAllTouchPoints()
{
    if (!m_pendingInputModeChange)
        return;

    if (RefPtr pageClient = this->pageClient())
        pageClient->focusedElementDidChangeInputMode(*m_pendingInputModeChange);
    m_pendingInputModeChange = std::nullopt;
}

void WebPageProxy::autofillLoginCredentials(const String& username, const String& password)
{
    m_legacyMainFrameProcess->send(Messages::WebPage::AutofillLoginCredentials(username, password), webPageIDInMainFrameProcess());
}

void WebPageProxy::showInspectorHighlight(const WebCore::InspectorOverlay::Highlight& highlight)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->showInspectorHighlight(highlight);
}

void WebPageProxy::hideInspectorHighlight()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->hideInspectorHighlight();
}

void WebPageProxy::showInspectorIndication()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->showInspectorIndication();
}

void WebPageProxy::hideInspectorIndication()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->hideInspectorIndication();
}

void WebPageProxy::enableInspectorNodeSearch()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->enableInspectorNodeSearch();
}

void WebPageProxy::disableInspectorNodeSearch()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->disableInspectorNodeSearch();
}

void WebPageProxy::focusNextFocusedElement(bool isForward, CompletionHandler<void()>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction();
        return;
    }
    
    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::FocusNextFocusedElement(isForward), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = m_legacyMainFrameProcess->throttler().backgroundActivity("WebPageProxy::focusNextFocusedElement"_s)] () mutable {
        callbackFunction();
    }, webPageIDInMainFrameProcess());
}

void WebPageProxy::setFocusedElementValue(const WebCore::ElementContext& context, const String& value)
{
    legacyMainFrameProcess().send(Messages::WebPage::SetFocusedElementValue(context, value), webPageIDInMainFrameProcess());
}

void WebPageProxy::setFocusedElementSelectedIndex(const WebCore::ElementContext& context, uint32_t index, bool allowMultipleSelection)
{
    legacyMainFrameProcess().send(Messages::WebPage::SetFocusedElementSelectedIndex(context, index, allowMultipleSelection), webPageIDInMainFrameProcess());
}

void WebPageProxy::didPerformDictionaryLookup(const DictionaryPopupInfo& dictionaryPopupInfo)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->didPerformDictionaryLookup(dictionaryPopupInfo);
}

void WebPageProxy::setRemoteLayerTreeRootNode(RemoteLayerTreeNode* rootNode)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->setRemoteLayerTreeRootNode(rootNode);
    m_frozenRemoteLayerTreeHost = nullptr;
}

void WebPageProxy::showPlaybackTargetPicker(bool hasVideo, const IntRect& elementRect, WebCore::RouteSharingPolicy policy, const String& contextUID)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->showPlaybackTargetPicker(hasVideo, elementRect, policy, contextUID);
}

void WebPageProxy::commitPotentialTapFailed()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->commitPotentialTapFailed();
}

void WebPageProxy::didNotHandleTapAsClick(const WebCore::IntPoint& point)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->didNotHandleTapAsClick(point);
    m_uiClient->didNotHandleTapAsClick(point);
}

void WebPageProxy::didHandleTapAsHover()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->didHandleTapAsHover();
}

void WebPageProxy::didCompleteSyntheticClick()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->didCompleteSyntheticClick();
}

void WebPageProxy::disableDoubleTapGesturesDuringTapIfNecessary(WebKit::TapIdentifier requestID)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->disableDoubleTapGesturesDuringTapIfNecessary(requestID);
}

void WebPageProxy::handleSmartMagnificationInformationForPotentialTap(WebKit::TapIdentifier requestID, const WebCore::FloatRect& renderRect, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale, bool nodeIsRootLevel)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->handleSmartMagnificationInformationForPotentialTap(requestID, renderRect, fitEntireRect, viewportMinimumScale, viewportMaximumScale, nodeIsRootLevel);
}

size_t WebPageProxy::computePagesForPrintingiOS(FrameIdentifier frameID, const PrintInfo& printInfo)
{
    ASSERT_WITH_MESSAGE(!printInfo.snapshotFirstPage, "If we are just snapshotting the first page, we don't need a synchronous message to determine the page count, which is 1.");

    if (!hasRunningProcess())
        return 0;

    auto sendResult = sendSyncToProcessContainingFrame(frameID, Messages::WebPage::ComputePagesForPrintingiOS(frameID, printInfo), Seconds::infinity());
    auto [pageCount] = sendResult.takeReplyOr(0);
    return pageCount;
}

std::optional<IPC::Connection::AsyncReplyID> WebPageProxy::drawToPDFiOS(FrameIdentifier frameID, const PrintInfo& printInfo, size_t pageCount, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler({ });
        return std::nullopt;
    }

    return sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::DrawToPDFiOS(frameID, printInfo, pageCount), WTFMove(completionHandler));
}

std::optional<IPC::Connection::AsyncReplyID> WebPageProxy::drawToImage(FrameIdentifier frameID, const PrintInfo& printInfo, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler({ });
        return std::nullopt;
    }

    return sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::DrawToImage(frameID, printInfo), WTFMove(completionHandler));
}

void WebPageProxy::contentSizeCategoryDidChange(const String& contentSizeCategory)
{
    legacyMainFrameProcess().send(Messages::WebPage::ContentSizeCategoryDidChange(contentSizeCategory), webPageIDInMainFrameProcess());
}

void WebPageProxy::generateSyntheticEditingCommand(WebKit::SyntheticEditingCommandType command)
{
    if (!hasRunningProcess())
        return;

    legacyMainFrameProcess().send(Messages::WebPage::GenerateSyntheticEditingCommand(command), webPageIDInMainFrameProcess());
}

void WebPageProxy::didUpdateEditorState(const EditorState& oldEditorState, const EditorState& newEditorState)
{
    bool couldChangeSecureInputState = newEditorState.isInPasswordField != oldEditorState.isInPasswordField || oldEditorState.selectionIsNone;
    
    // Selection being none is a temporary state when editing. Flipping secure input state too quickly was causing trouble (not fully understood).
    if (couldChangeSecureInputState && !newEditorState.selectionIsNone) {
        if (RefPtr pageClient = this->pageClient())
            pageClient->updateSecureInputState();
    }
    
    if (newEditorState.shouldIgnoreSelectionChanges)
        return;
    
    updateFontAttributesAfterEditorStateChange();
    // We always need to notify the client on iOS to make sure the selection is redrawn,
    // even during composition to support phrase boundary gesture.
    if (RefPtr pageClient = this->pageClient())
        pageClient->selectionDidChange();
}

void WebPageProxy::dispatchDidUpdateEditorState()
{
    if (!m_waitingForPostLayoutEditorStateUpdateAfterFocusingElement || !editorState().hasPostLayoutData())
        return;

    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    pageClient->didUpdateEditorState();
    m_waitingForPostLayoutEditorStateUpdateAfterFocusingElement = false;
}

void WebPageProxy::showValidationMessage(const IntRect& anchorClientRect, const String& message)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    m_validationBubble = pageClient->createValidationBubble(message, { m_preferences->minimumFontSize() });

    // FIXME: When in element fullscreen, UIClient::presentingViewController() may not return the
    // WKFullScreenViewController even though that is the presenting view controller of the WKWebView.
    // We should call PageClientImpl::presentingViewController() instead.
    m_validationBubble->setAnchorRect(anchorClientRect, uiClient().presentingViewController());

    // If we are currently doing a scrolling / zoom animation, then we'll delay showing the validation
    // bubble until the animation is over.
    if (!m_isScrollingOrZooming)
        m_validationBubble->show();
}

void WebPageProxy::setIsScrollingOrZooming(bool isScrollingOrZooming)
{
    m_isScrollingOrZooming = isScrollingOrZooming;

    // We finished doing the scrolling / zoom animation so we can now show the validation
    // bubble if we're supposed to.
    if (!m_isScrollingOrZooming && m_validationBubble)
        m_validationBubble->show();
}

void WebPageProxy::hardwareKeyboardAvailabilityChanged(HardwareKeyboardState hardwareKeyboardState)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->hardwareKeyboardAvailabilityChanged();

    updateCurrentModifierState();
    m_legacyMainFrameProcess->send(Messages::WebPage::HardwareKeyboardAvailabilityChanged(hardwareKeyboardState), webPageIDInMainFrameProcess());
}

void WebPageProxy::requestEvasionRectsAboveSelection(CompletionHandler<void(const Vector<WebCore::FloatRect>&)>&& callback)
{
    if (!hasRunningProcess()) {
        callback({ });
        return;
    }

    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::RequestEvasionRectsAboveSelection(), WTFMove(callback), webPageIDInMainFrameProcess());
}

void WebPageProxy::updateSelectionWithDelta(int64_t locationDelta, int64_t lengthDelta, CompletionHandler<void()>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler();
        return;
    }

    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::UpdateSelectionWithDelta(locationDelta, lengthDelta), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

WebCore::FloatRect WebPageProxy::selectionBoundingRectInRootViewCoordinates() const
{
    if (editorState().selectionIsNone)
        return { };

    if (!editorState().hasVisualData())
        return { };

    WebCore::FloatRect bounds;
    auto& visualData = *editorState().visualData;
    if (editorState().selectionIsRange) {
        for (auto& geometry : visualData.selectionGeometries)
            bounds.unite(geometry.rect());
    } else
        bounds = visualData.caretRectAtStart;

    return bounds;
}

void WebPageProxy::requestDocumentEditingContext(WebKit::DocumentEditingContextRequest&& request, CompletionHandler<void(WebKit::DocumentEditingContext&&)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler({ });
        return;
    }

    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::RequestDocumentEditingContext(WTFMove(request)), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

#if ENABLE(DRAG_SUPPORT)

void WebPageProxy::didHandleDragStartRequest(bool started)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->didHandleDragStartRequest(started);
}

void WebPageProxy::didHandleAdditionalDragItemsRequest(bool added)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->didHandleAdditionalDragItemsRequest(added);
}

void WebPageProxy::requestDragStart(const WebCore::IntPoint& clientPosition, const WebCore::IntPoint& globalPosition, OptionSet<WebCore::DragSourceAction> allowedActionsMask)
{
    if (hasRunningProcess())
        m_legacyMainFrameProcess->send(Messages::WebPage::RequestDragStart(clientPosition, globalPosition, allowedActionsMask), webPageIDInMainFrameProcess());
}

void WebPageProxy::requestAdditionalItemsForDragSession(const IntPoint& clientPosition, const IntPoint& globalPosition, OptionSet<WebCore::DragSourceAction> allowedActionsMask)
{
    if (hasRunningProcess())
        m_legacyMainFrameProcess->send(Messages::WebPage::RequestAdditionalItemsForDragSession(clientPosition, globalPosition, allowedActionsMask), webPageIDInMainFrameProcess());
}

void WebPageProxy::insertDroppedImagePlaceholders(const Vector<IntSize>& imageSizes, CompletionHandler<void(const Vector<IntRect>&, std::optional<WebCore::TextIndicatorData>)>&& completionHandler)
{
    if (hasRunningProcess())
        legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::InsertDroppedImagePlaceholders(imageSizes), WTFMove(completionHandler), webPageIDInMainFrameProcess());
    else
        completionHandler({ }, std::nullopt);
}

void WebPageProxy::willReceiveEditDragSnapshot()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->willReceiveEditDragSnapshot();
}

void WebPageProxy::didReceiveEditDragSnapshot(std::optional<TextIndicatorData> data)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->didReceiveEditDragSnapshot(data);
}

void WebPageProxy::didConcludeDrop()
{
    m_legacyMainFrameProcess->send(Messages::WebPage::DidConcludeDrop(), webPageIDInMainFrameProcess());
}

#endif

#if USE(QUICK_LOOK)

void WebPageProxy::didStartLoadForQuickLookDocumentInMainFrame(const String& fileName, const String& uti)
{
    // Ensure that fileName isn't really a path name
    static_assert(notFound + 1 == 0, "The following line assumes WTF::notFound equals -1");
    m_navigationClient->didStartLoadForQuickLookDocumentInMainFrame(fileName.substring(fileName.reverseFind('/') + 1), uti);
}

void WebPageProxy::didFinishLoadForQuickLookDocumentInMainFrame(ShareableResource::Handle&& handle)
{
    auto buffer = WTFMove(handle).tryWrapInSharedBuffer();
    if (!buffer)
        return;

    m_navigationClient->didFinishLoadForQuickLookDocumentInMainFrame(*buffer);
}

void WebPageProxy::requestPasswordForQuickLookDocumentInMainFrame(const String& fileName, CompletionHandler<void(const String&)>&& completionHandler)
{
    requestPasswordForQuickLookDocumentInMainFrameShared(fileName, WTFMove(completionHandler));
}

void WebPageProxy::requestPasswordForQuickLookDocumentInMainFrameShared(const String& fileName, CompletionHandler<void(const String&)>&& completionHandler)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return completionHandler({ });
    pageClient->requestPasswordForQuickLookDocument(fileName, WTFMove(completionHandler));
}

#endif

#if ENABLE(APPLE_PAY)

Ref<PaymentAuthorizationPresenter> WebPageProxy::Internals::paymentCoordinatorAuthorizationPresenter(WebPaymentCoordinatorProxy& paymentCoordinatorProxy, PKPaymentRequest *paymentRequest)
{
    return PaymentAuthorizationController::create(paymentCoordinatorProxy, paymentRequest);
}

UIViewController *WebPageProxy::Internals::paymentCoordinatorPresentingViewController(const WebPaymentCoordinatorProxy&)
{
    // FIXME: When in element fullscreen, UIClient::presentingViewController() may not return the
    // WKFullScreenViewController even though that is the presenting view controller of the WKWebView.
    // We should call PageClientImpl::presentingViewController() instead.
    return page->uiClient().presentingViewController();
}

const String& WebPageProxy::Internals::paymentCoordinatorCTDataConnectionServiceType(const WebPaymentCoordinatorProxy&)
{
    return page->websiteDataStore().configuration().dataConnectionServiceType();
}

#endif

#if ENABLE(APPLE_PAY) && ENABLE(APPLE_PAY_REMOTE_UI_USES_SCENE)

void WebPageProxy::Internals::getWindowSceneAndBundleIdentifierForPaymentPresentation(WebPageProxyIdentifier, CompletionHandler<void(const String&, const String&)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(nullString(), nullString());
}

#endif

static bool desktopClassBrowsingSupported()
{
    static bool supportsDesktopClassBrowsing = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        supportsDesktopClassBrowsing = !PAL::currentUserInterfaceIdiomIsSmallScreen();
    });
    return supportsDesktopClassBrowsing;
}

#if !PLATFORM(MACCATALYST)

static bool webViewSizeIsNarrow(WebCore::IntSize viewSize)
{
    return !viewSize.isEmpty() && viewSize.width() <= 375;
}

#endif // !PLATFORM(MACCATALYST)

enum class RecommendDesktopClassBrowsingForRequest { No, Yes, Auto };
static RecommendDesktopClassBrowsingForRequest desktopClassBrowsingRecommendedForRequest(const WebCore::ResourceRequest& request)
{
    // FIXME: This should be additionally gated on site-specific quirks being enabled.
    // See also: <rdar://problem/50035167>.
    // The list of domain names is currently available in Source/WebCore/page/Quirks.cpp
    if (Quirks::needsIPadMiniUserAgent(request.url()))
        return RecommendDesktopClassBrowsingForRequest::No;

    if (Quirks::needsIPhoneUserAgent(request.url()))
        return RecommendDesktopClassBrowsingForRequest::No;

    if (Quirks::needsDesktopUserAgent(request.url()))
        return RecommendDesktopClassBrowsingForRequest::Yes;

    return RecommendDesktopClassBrowsingForRequest::Auto;
}

bool WebPageProxy::isDesktopClassBrowsingRecommended(const WebCore::ResourceRequest& request) const
{
    auto desktopClassBrowsingRecommendation = m_preferences->needsSiteSpecificQuirks() ? desktopClassBrowsingRecommendedForRequest(request) : RecommendDesktopClassBrowsingForRequest::Auto;
    if (desktopClassBrowsingRecommendation == RecommendDesktopClassBrowsingForRequest::Yes)
        return true;

    if (desktopClassBrowsingRecommendation == RecommendDesktopClassBrowsingForRequest::No)
        return false;

#if !PLATFORM(MACCATALYST)
    if (RefPtr pageClient = this->pageClient(); pageClient && !pageClient->hasResizableWindows() && webViewSizeIsNarrow(viewSize()))
        return false;
#endif

    static bool shouldRecommendDesktopClassBrowsing = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
#if PLATFORM(MACCATALYST)
        shouldRecommendDesktopClassBrowsing = desktopClassBrowsingSupported();
#else
        // While desktop-class browsing is supported on all iPad models, it is not recommended for iPad mini.
        auto screenClass = MGGetSInt32Answer(kMGQMainScreenClass, MGScreenClassPad2);
        shouldRecommendDesktopClassBrowsing = screenClass != MGScreenClassPad3 && screenClass != MGScreenClassPad4 && desktopClassBrowsingSupported();
#endif
        if (!m_navigationClient->shouldBypassContentModeSafeguards() && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ModernCompabilityModeByDefault)) {
            // Opt out apps that haven't yet built against the iOS 13 SDK to limit any incompatibilities as a result of enabling desktop-class browsing by default in
            // WKWebView on appropriately-sized iPad models.
            shouldRecommendDesktopClassBrowsing = false;
        }
    });
    return shouldRecommendDesktopClassBrowsing;
}

bool WebPageProxy::useDesktopClassBrowsing(const API::WebsitePolicies& policies, const WebCore::ResourceRequest& request) const
{
    switch (policies.preferredContentMode()) {
    case WebContentMode::Recommended: {
        return isDesktopClassBrowsingRecommended(request);
    }
    case WebContentMode::Mobile:
        return false;
    case WebContentMode::Desktop:
        return !policies.allowSiteSpecificQuirksToOverrideContentMode() || desktopClassBrowsingRecommendedForRequest(request) != RecommendDesktopClassBrowsingForRequest::No;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

String WebPageProxy::predictedUserAgentForRequest(const WebCore::ResourceRequest& request) const
{
    if (!customUserAgent().isEmpty())
        return customUserAgent();

    const API::WebsitePolicies& policies = m_configuration->defaultWebsitePolicies();
    if (!policies.customUserAgent().isEmpty())
        return policies.customUserAgent();

    if (policies.applicationNameForDesktopUserAgent().isEmpty())
        return userAgent();

    if (!useDesktopClassBrowsing(policies, request))
        return userAgent();

    return standardUserAgentWithApplicationName(policies.applicationNameForDesktopUserAgent(), emptyString(), UserAgentType::Desktop);
}

WebContentMode WebPageProxy::effectiveContentModeAfterAdjustingPolicies(API::WebsitePolicies& policies, const WebCore::ResourceRequest& request)
{
    if (m_preferences->mediaSourceEnabled()) {
        // FIXME: This is a compatibility hack to ensure that turning MSE on via the existing preference still enables MSE.
        policies.setMediaSourcePolicy(WebsiteMediaSourcePolicy::Enable);
    }

    if (auto selectors = Quirks::defaultVisibilityAdjustmentSelectors(request.url()))
        policies.setVisibilityAdjustmentSelectors({ WTFMove(*selectors) });

    if (Quirks::needsIPhoneUserAgent(request.url())) {
        policies.setCustomUserAgent(makeStringByReplacingAll(standardUserAgentWithApplicationName(m_applicationNameForUserAgent), "iPad"_s, "iPhone"_s));
        policies.setCustomNavigatorPlatform("iPhone"_s);
        return WebContentMode::Mobile;
    }

    bool useDesktopBrowsingMode = useDesktopClassBrowsing(policies, request);

    m_preferFasterClickOverDoubleTap = false;

    if (!useDesktopBrowsingMode) {
        policies.setIdempotentModeAutosizingOnlyHonorsPercentages(true);
        return WebContentMode::Mobile;
    }

    if (policies.customUserAgent().isEmpty() && customUserAgent().isEmpty()) {
        auto applicationName = policies.applicationNameForDesktopUserAgent();
        if (applicationName.isEmpty())
            applicationName = applicationNameForDesktopUserAgent();
        policies.setCustomUserAgent(standardUserAgentWithApplicationName(applicationName, emptyString(), UserAgentType::Desktop));
    }

    if (policies.customNavigatorPlatform().isEmpty()) {
        // FIXME: Grab this from WebCore instead of hard-coding it here.
        policies.setCustomNavigatorPlatform("MacIntel"_s);
    }

    if (desktopClassBrowsingSupported()) {
        // Apply some additional desktop-class browsing behaviors on supported devices.
        policies.setMetaViewportPolicy(WebsiteMetaViewportPolicy::Ignore);
        policies.setMediaSourcePolicy(WebsiteMediaSourcePolicy::Enable);
        policies.setSimulatedMouseEventsDispatchPolicy(WebsiteSimulatedMouseEventsDispatchPolicy::Allow);
        policies.setLegacyOverflowScrollingTouchPolicy(WebsiteLegacyOverflowScrollingTouchPolicy::Disable);
        m_preferFasterClickOverDoubleTap = true;
    }

#if ENABLE(TOUCH_EVENTS)
    if (m_preferences->needsSiteSpecificQuirks() && Quirks::shouldOmitTouchEventDOMAttributesForDesktopWebsite(request.url()))
        policies.setOverrideTouchEventDOMAttributesEnabled(false);
#endif

    return WebContentMode::Desktop;
}

void WebPageProxy::textInputContextsInRect(FloatRect rect, CompletionHandler<void(const Vector<ElementContext>&)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler({ });
        return;
    }

    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::TextInputContextsInRect(rect), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::focusTextInputContextAndPlaceCaret(const ElementContext& context, const IntPoint& point, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler(false);
        return;
    }

    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::FocusTextInputContextAndPlaceCaret(context, point), WTFMove(completionHandler), webPageIDInMainFrameProcess());
}

void WebPageProxy::setShouldRevealCurrentSelectionAfterInsertion(bool shouldRevealCurrentSelectionAfterInsertion)
{
    if (hasRunningProcess())
        legacyMainFrameProcess().send(Messages::WebPage::SetShouldRevealCurrentSelectionAfterInsertion(shouldRevealCurrentSelectionAfterInsertion), webPageIDInMainFrameProcess());
}

void WebPageProxy::setScreenIsBeingCaptured(bool captured)
{
    if (hasRunningProcess())
        legacyMainFrameProcess().send(Messages::WebPage::SetScreenIsBeingCaptured(captured), webPageIDInMainFrameProcess());
}

void WebPageProxy::willOpenAppLink()
{
    if (hasValidOpeningAppLinkActivity())
        return;

    // We take a background activity for 25 seconds when switching to another app via an app link in case the WebPage
    // needs to run script to pass information to the native app.
    // We chose 25 seconds because the system only gives us 30 seconds and we don't want to get too close to that limit
    // to avoid assertion invalidation (or even termination).
    takeOpeningAppLinkActivity();
    WorkQueue::main().dispatchAfter(25_s, [weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->dropOpeningAppLinkActivity();
    });
}

void WebPageProxy::processWillBecomeSuspended()
{
    if (!hasRunningProcess())
        return;

    m_hasNetworkRequestsOnSuspended = pageLoadState().networkRequestsInProgress();
    if (m_hasNetworkRequestsOnSuspended)
        setNetworkRequestsInProgress(false);
}

void WebPageProxy::processWillBecomeForeground()
{
    if (!hasRunningProcess())
        return;

    if (m_hasNetworkRequestsOnSuspended) {
        setNetworkRequestsInProgress(true);
        m_hasNetworkRequestsOnSuspended = false;
    }
}

#if PLATFORM(MACCATALYST)

void WebPageProxy::Internals::isUserFacingChanged(bool isUserFacing)
{
    if (!isUserFacing)
        protectedPage()->suspendAllMediaPlayback([] { });
    else
        protectedPage()->resumeAllMediaPlayback([] { });
}

#endif

std::optional<IPC::AsyncReplyID> WebPageProxy::willPerformPasteCommand(DOMPasteAccessCategory pasteAccessCategory, CompletionHandler<void()>&& completionHandler, std::optional<FrameIdentifier> frameID)
{
    switch (pasteAccessCategory) {
    case DOMPasteAccessCategory::General:
    case DOMPasteAccessCategory::Fonts:
        return grantAccessToCurrentPasteboardData(UIPasteboardNameGeneral, WTFMove(completionHandler), frameID);
    }
}

void WebPageProxy::setDeviceHasAGXCompilerServiceForTesting() const
{
    WebCore::setDeviceHasAGXCompilerServiceForTesting();
}

void WebPageProxy::showDataDetectorsUIForPositionInformation(const InteractionInformationAtPosition& positionInfo)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->showDataDetectorsUIForPositionInformation(positionInfo);
}

void WebPageProxy::insertionPointColorDidChange()
{
    if (RefPtr pageClient = this->pageClient())
        legacyMainFrameProcess().send(Messages::WebPage::SetInsertionPointColor(pageClient->insertionPointColor()), webPageIDInMainFrameProcess());
}

void WebPageProxy::shouldDismissKeyboardAfterTapAtPoint(FloatPoint point, CompletionHandler<void(bool)>&& completion)
{
    if (!hasRunningProcess())
        return completion(false);

    legacyMainFrameProcess().sendWithAsyncReply(Messages::WebPage::ShouldDismissKeyboardAfterTapAtPoint(point), WTFMove(completion), webPageIDInMainFrameProcess());
}

Color WebPageProxy::platformUnderPageBackgroundColor() const
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return WebCore::Color::white;

    if (auto contentViewBackgroundColor = pageClient->contentViewBackgroundColor(); contentViewBackgroundColor.isValid())
        return contentViewBackgroundColor;

    return WebCore::Color::white;
}

void WebPageProxy::statusBarWasTapped()
{
#if PLATFORM(IOS) || PLATFORM(VISION)
    RELEASE_LOG_INFO(WebRTC, "WebPageProxy::statusBarWasTapped");

#if USE(APPLE_INTERNAL_SDK)
    UIApplication *app = UIApplication.sharedApplication;
    if (!app.supportsMultipleScenes && app.applicationState != UIApplicationStateActive)
        [[LSApplicationWorkspace defaultWorkspace] openApplicationWithBundleID:[[NSBundle mainBundle] bundleIdentifier]];
#endif

    m_uiClient->statusBarWasTapped();
#endif
}

double WebPageProxy::displayedContentScale() const
{
    return internals().lastVisibleContentRectUpdate.scale();
}

const FloatRect& WebPageProxy::exposedContentRect() const
{
    return internals().lastVisibleContentRectUpdate.exposedContentRect();
}

const FloatRect& WebPageProxy::unobscuredContentRect() const
{
    return internals().lastVisibleContentRectUpdate.unobscuredContentRect();
}

bool WebPageProxy::inStableState() const
{
    return internals().lastVisibleContentRectUpdate.inStableState();
}

const FloatRect& WebPageProxy::unobscuredContentRectRespectingInputViewBounds() const
{
    return internals().lastVisibleContentRectUpdate.unobscuredContentRectRespectingInputViewBounds();
}

const FloatRect& WebPageProxy::layoutViewportRect() const
{
    return internals().lastVisibleContentRectUpdate.layoutViewportRect();
}

FloatSize WebPageProxy::viewLayoutSize() const
{
    return internals().viewportConfigurationViewLayoutSize;
}

#if ENABLE(DRAG_SUPPORT)

void WebPageProxy::setPromisedDataForImage(IPC::Connection&, const String&, SharedMemory::Handle&&, const String&, const String&, const String&, const String&, const String&, SharedMemory::Handle&&, const String&)
{
    notImplemented();
}

#if ENABLE(PDF_PLUGIN)
void WebPageProxy::pluginDidInstallPDFDocument(double initialScale)
{
    protectedPageClient()->pluginDidInstallPDFDocument(initialScale);
}
#endif

#endif

} // namespace WebKit

#undef WEBPAGEPROXY_RELEASE_LOG
#undef MESSAGE_CHECK

#endif // PLATFORM(IOS_FAMILY)
