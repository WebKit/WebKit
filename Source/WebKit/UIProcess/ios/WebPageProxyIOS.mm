/*
 * Copyright (C) 2012-2022 Apple Inc. All rights reserved.
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
#import "DataReference.h"
#import "DocumentEditingContext.h"
#import "EditingRange.h"
#import "GlobalFindInPageState.h"
#import "InteractionInformationAtPosition.h"
#import "Logging.h"
#import "NativeWebKeyboardEvent.h"
#import "NavigationState.h"
#import "PageClient.h"
#import "PaymentAuthorizationViewController.h"
#import "PrintInfo.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteLayerTreeDrawingAreaProxyMessages.h"
#import "RemoteLayerTreeTransaction.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "RevealItem.h"
#import "ShareableResource.h"
#import "TapHandlingResult.h"
#import "UIKitSPI.h"
#import "UserData.h"
#import "UserInterfaceIdiom.h"
#import "VideoFullscreenManagerProxy.h"
#import "ViewUpdateDispatcherMessages.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WebAutocorrectionContext.h"
#import "WebAutocorrectionData.h"
#import "WebPageMessages.h"
#import "WebProcessMessages.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import <WebCore/AGXCompilerService.h>
#import <WebCore/FrameView.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/UserAgent.h>
#import <WebCore/ValidationBubble.h>
#import <pal/spi/ios/MobileGestaltSPI.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/text/TextStream.h>

#if USE(QUICK_LOOK)
#import "APILoaderClient.h"
#import "APINavigationClient.h"
#import <wtf/text/WTFString.h>
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, process().connection())

#define WEBPAGEPROXY_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [pageProxyID=%llu, webPageID=%llu, PID=%i] WebPageProxy::" fmt, this, m_identifier.toUInt64(), m_webPageID.toUInt64(), m_process->processIdentifier(), ##__VA_ARGS__)

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
    return std::numeric_limits<uint32_t>::max() - webPageID().toUInt64();
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

void WebPageProxy::searchWithSpotlight(const String&)
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

    sendWithAsyncReply(Messages::WebPage::RequestFocusedElementInformation(), WTFMove(callback));
}

void WebPageProxy::updateVisibleContentRects(const VisibleContentRectUpdateInfo& visibleContentRectUpdate, bool sendEvenIfUnchanged)
{
    if (visibleContentRectUpdate == m_lastVisibleContentRectUpdate && !sendEvenIfUnchanged)
        return;

    m_lastVisibleContentRectUpdate = visibleContentRectUpdate;

    if (!hasRunningProcess())
        return;

    m_process->send(Messages::ViewUpdateDispatcher::VisibleContentRectUpdate(m_webPageID, visibleContentRectUpdate), 0);
}

void WebPageProxy::resendLastVisibleContentRects()
{
    m_process->send(Messages::ViewUpdateDispatcher::VisibleContentRectUpdate(m_webPageID, m_lastVisibleContentRectUpdate), 0);
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

WebCore::FloatRect WebPageProxy::computeLayoutViewportRect(const FloatRect& unobscuredContentRect, const FloatRect& unobscuredContentRectRespectingInputViewBounds, const FloatRect& currentLayoutViewportRect, double displayedContentScale, FrameView::LayoutViewportConstraint constraint) const
{
    FloatRect constrainedUnobscuredRect = unobscuredContentRect;
    FloatRect documentRect = pageClient().documentRect();

    if (constraint == FrameView::LayoutViewportConstraint::ConstrainedToDocumentRect)
        constrainedUnobscuredRect.intersect(documentRect);

    double minimumScale = pageClient().minimumZoomScale();
    bool isBelowMinimumScale = displayedContentScale < minimumScale;
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

    auto layoutViewportSize = FrameView::expandedLayoutViewportSize(m_baseLayoutViewportSize, LayoutSize(documentRect.size()), m_preferences->layoutViewportHeightExpansionFactor());
    FloatRect layoutViewportRect = FrameView::computeUpdatedLayoutViewportRect(LayoutRect(currentLayoutViewportRect), LayoutRect(documentRect), LayoutSize(constrainedSize), LayoutRect(unobscuredContentRectForViewport), layoutViewportSize, m_minStableLayoutViewportOrigin, m_maxStableLayoutViewportOrigin, constraint);
    
    if (layoutViewportRect != currentLayoutViewportRect)
        LOG_WITH_STREAM(VisibleRects, stream << "WebPageProxy::computeLayoutViewportRect: new layout viewport  " << layoutViewportRect);
    return layoutViewportRect;
}

FloatRect WebPageProxy::unconstrainedLayoutViewportRect() const
{
    return computeLayoutViewportRect(unobscuredContentRect(), unobscuredContentRectRespectingInputViewBounds(), layoutViewportRect(), displayedContentScale(), FrameView::LayoutViewportConstraint::Unconstrained);
}

void WebPageProxy::adjustLayersForLayoutViewport(const FloatRect& layoutViewport)
{
    if (!m_scrollingCoordinatorProxy)
        return;

    m_scrollingCoordinatorProxy->viewportChangedViaDelegatedScrolling(unobscuredContentRect().location(), layoutViewport, displayedContentScale());
}

void WebPageProxy::scrollingNodeScrollViewWillStartPanGesture(ScrollingNodeID nodeID)
{
    pageClient().scrollingNodeScrollViewWillStartPanGesture(nodeID);
}

void WebPageProxy::scrollingNodeScrollViewDidScroll(ScrollingNodeID nodeID)
{
    pageClient().scrollingNodeScrollViewDidScroll(nodeID);
}

void WebPageProxy::scrollingNodeScrollWillStartScroll(ScrollingNodeID nodeID)
{
    pageClient().scrollingNodeScrollWillStartScroll(nodeID);
}

void WebPageProxy::scrollingNodeScrollDidEndScroll(ScrollingNodeID nodeID)
{
    pageClient().scrollingNodeScrollDidEndScroll(nodeID);
}

void WebPageProxy::dynamicViewportSizeUpdate(const FloatSize& viewLayoutSize, const WebCore::FloatSize& minimumUnobscuredSize, const WebCore::FloatSize& maximumUnobscuredSize, const FloatRect& targetExposedContentRect, const FloatRect& targetUnobscuredRect, const FloatRect& targetUnobscuredRectInScrollViewCoordinates, const WebCore::FloatBoxExtent& unobscuredSafeAreaInsets, double targetScale, int32_t deviceOrientation, double minimumEffectiveDeviceWidth, DynamicViewportSizeUpdateID dynamicViewportSizeUpdateID)
{
    if (!hasRunningProcess())
        return;

    hideValidationMessage();

    m_viewportConfigurationViewLayoutSize = viewLayoutSize;
    m_defaultUnobscuredSize = maximumUnobscuredSize;
    m_minimumUnobscuredSize = minimumUnobscuredSize;
    m_maximumUnobscuredSize = maximumUnobscuredSize;
    m_process->send(Messages::WebPage::DynamicViewportSizeUpdate(viewLayoutSize,
        minimumUnobscuredSize, maximumUnobscuredSize, targetExposedContentRect, targetUnobscuredRect,
        targetUnobscuredRectInScrollViewCoordinates, unobscuredSafeAreaInsets,
        targetScale, deviceOrientation, minimumEffectiveDeviceWidth, dynamicViewportSizeUpdateID), m_webPageID);

    setDeviceOrientation(deviceOrientation);
}

void WebPageProxy::setViewportConfigurationViewLayoutSize(const WebCore::FloatSize& size, double scaleFactor, double minimumEffectiveDeviceWidth)
{
    m_viewportConfigurationViewLayoutSize = size;
    m_viewportConfigurationLayoutSizeScaleFactor = scaleFactor;
    m_viewportConfigurationMinimumEffectiveDeviceWidth = minimumEffectiveDeviceWidth;

    if (hasRunningProcess())
        m_process->send(Messages::WebPage::SetViewportConfigurationViewLayoutSize(size, scaleFactor, minimumEffectiveDeviceWidth), m_webPageID);
}

void WebPageProxy::setForceAlwaysUserScalable(bool userScalable)
{
    if (m_forceAlwaysUserScalable == userScalable)
        return;
    m_forceAlwaysUserScalable = userScalable;

    if (hasRunningProcess())
        m_process->send(Messages::WebPage::SetForceAlwaysUserScalable(userScalable), m_webPageID);
}

void WebPageProxy::setDeviceOrientation(int32_t deviceOrientation)
{
    if (deviceOrientation != m_deviceOrientation) {
        m_deviceOrientation = deviceOrientation;
        if (hasRunningProcess()) {
            m_process->send(Messages::WebPage::SetDeviceOrientation(deviceOrientation), m_webPageID);
            setOrientationForMediaCapture(deviceOrientation);
        }
    }
}

void WebPageProxy::setOverrideViewportArguments(const std::optional<ViewportArguments>& viewportArguments)
{
    if (viewportArguments == m_overrideViewportArguments)
        return;

    m_overrideViewportArguments = viewportArguments;

    if (hasRunningProcess())
        m_process->send(Messages::WebPage::SetOverrideViewportArguments(viewportArguments), m_webPageID);
}

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

bool WebPageProxy::updateLayoutViewportParameters(const WebKit::RemoteLayerTreeTransaction& layerTreeTransaction)
{
    if (m_baseLayoutViewportSize == layerTreeTransaction.baseLayoutViewportSize()
        && m_minStableLayoutViewportOrigin == layerTreeTransaction.minStableLayoutViewportOrigin()
        && m_maxStableLayoutViewportOrigin == layerTreeTransaction.maxStableLayoutViewportOrigin())
        return false;

    m_baseLayoutViewportSize = layerTreeTransaction.baseLayoutViewportSize();
    m_minStableLayoutViewportOrigin = layerTreeTransaction.minStableLayoutViewportOrigin();
    m_maxStableLayoutViewportOrigin = layerTreeTransaction.maxStableLayoutViewportOrigin();

    LOG_WITH_STREAM(VisibleRects, stream << "WebPageProxy::updateLayoutViewportParameters: baseLayoutViewportSize: " << m_baseLayoutViewportSize << " minStableLayoutViewportOrigin: " << m_minStableLayoutViewportOrigin << " maxStableLayoutViewportOrigin: " << m_maxStableLayoutViewportOrigin);

    return true;
}

void WebPageProxy::layerTreeCommitComplete()
{
    pageClient().layerTreeCommitComplete();
}

void WebPageProxy::selectWithGesture(const WebCore::IntPoint point, GestureType gestureType, GestureRecognizerState gestureState, bool isInteractingWithFocusedElement, CompletionHandler<void(const WebCore::IntPoint&, GestureType, GestureRecognizerState, OptionSet<WebKit::SelectionFlags>)>&& callback)
{
    if (!hasRunningProcess())
        return callback({ }, GestureType::Loupe, GestureRecognizerState::Possible, { });

    sendWithAsyncReply(Messages::WebPage::SelectWithGesture(point, gestureType, gestureState, isInteractingWithFocusedElement), WTFMove(callback));
}

void WebPageProxy::updateSelectionWithTouches(const WebCore::IntPoint point, SelectionTouch touches, bool baseIsStart, CompletionHandler<void(const WebCore::IntPoint&, SelectionTouch, OptionSet<SelectionFlags>)>&& callback)
{
    if (!hasRunningProcess())
        return callback(WebCore::IntPoint(), SelectionTouch::Started, { });

    sendWithAsyncReply(Messages::WebPage::UpdateSelectionWithTouches(point, touches, baseIsStart), WTFMove(callback));
}

void WebPageProxy::willInsertFinalDictationResult()
{
    m_process->send(Messages::WebPage::WillInsertFinalDictationResult(), m_webPageID);
}

void WebPageProxy::didInsertFinalDictationResult()
{
    m_process->send(Messages::WebPage::DidInsertFinalDictationResult(), m_webPageID);
}
    
void WebPageProxy::replaceDictatedText(const String& oldText, const String& newText)
{
    m_process->send(Messages::WebPage::ReplaceDictatedText(oldText, newText), m_webPageID);
}

void WebPageProxy::replaceSelectedText(const String& oldText, const String& newText)
{
    m_process->send(Messages::WebPage::ReplaceSelectedText(oldText, newText), m_webPageID);
}

void WebPageProxy::insertTextPlaceholder(const IntSize& size, CompletionHandler<void(const std::optional<ElementContext>&)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler({ });
        return;
    }
    sendWithAsyncReply(Messages::WebPage::InsertTextPlaceholder { size }, WTFMove(completionHandler));
}

void WebPageProxy::removeTextPlaceholder(const ElementContext& placeholder, CompletionHandler<void()>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler();
        return;
    }
    sendWithAsyncReply(Messages::WebPage::RemoveTextPlaceholder { placeholder }, WTFMove(completionHandler));
}

void WebPageProxy::requestAutocorrectionData(const String& textForAutocorrection, CompletionHandler<void(WebAutocorrectionData)>&& callback)
{
    if (!hasRunningProcess()) {
        callback({ });
        return;
    }
    sendWithAsyncReply(Messages::WebPage::RequestAutocorrectionData(textForAutocorrection), WTFMove(callback));
}

void WebPageProxy::applyAutocorrection(const String& correction, const String& originalText, CompletionHandler<void(const String&)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::ApplyAutocorrection(correction, originalText), WTFMove(callback));
}

bool WebPageProxy::applyAutocorrection(const String& correction, const String& originalText)
{
    auto sendSync = m_process->sendSync(Messages::WebPage::SyncApplyAutocorrection(correction, originalText), m_webPageID);
    auto [autocorrectionApplied] = sendSync.takeReplyOr(false);
    return autocorrectionApplied;
}

void WebPageProxy::selectTextWithGranularityAtPoint(const WebCore::IntPoint point, WebCore::TextGranularity granularity, bool isInteractingWithFocusedElement, CompletionHandler<void()>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction();
        return;
    }
    
    sendWithAsyncReply(Messages::WebPage::SelectTextWithGranularityAtPoint(point, granularity, isInteractingWithFocusedElement), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = m_process->throttler().backgroundActivity("WebPageProxy::selectTextWithGranularityAtPoint"_s)] () mutable {
        callbackFunction();
    });
}

void WebPageProxy::selectPositionAtBoundaryWithDirection(const WebCore::IntPoint point, WebCore::TextGranularity granularity, WebCore::SelectionDirection direction, bool isInteractingWithFocusedElement, CompletionHandler<void()>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction();
        return;
    }
    
    sendWithAsyncReply(Messages::WebPage::SelectPositionAtBoundaryWithDirection(point, granularity, direction, isInteractingWithFocusedElement), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = m_process->throttler().backgroundActivity("WebPageProxy::selectPositionAtBoundaryWithDirection"_s)] () mutable {
        callbackFunction();
    });
}

void WebPageProxy::moveSelectionAtBoundaryWithDirection(WebCore::TextGranularity granularity, WebCore::SelectionDirection direction, CompletionHandler<void()>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction();
        return;
    }
    
    sendWithAsyncReply(Messages::WebPage::MoveSelectionAtBoundaryWithDirection(granularity, direction), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = m_process->throttler().backgroundActivity("WebPageProxy::moveSelectionAtBoundaryWithDirection"_s)] () mutable {
        callbackFunction();
    });
}
    
void WebPageProxy::selectPositionAtPoint(const WebCore::IntPoint point, bool isInteractingWithFocusedElement, CompletionHandler<void()>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction();
        return;
    }
    
    sendWithAsyncReply(Messages::WebPage::SelectPositionAtPoint(point, isInteractingWithFocusedElement), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = m_process->throttler().backgroundActivity("WebPageProxy::selectPositionAtPoint"_s)] () mutable {
        callbackFunction();
    });
}

void WebPageProxy::beginSelectionInDirection(WebCore::SelectionDirection direction, CompletionHandler<void(bool)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::BeginSelectionInDirection(direction), WTFMove(callback));
}

void WebPageProxy::updateSelectionWithExtentPoint(const WebCore::IntPoint point, bool isInteractingWithFocusedElement, RespectSelectionAnchor respectSelectionAnchor, CompletionHandler<void(bool)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::UpdateSelectionWithExtentPoint(point, isInteractingWithFocusedElement, respectSelectionAnchor), WTFMove(callback));
}

void WebPageProxy::updateSelectionWithExtentPointAndBoundary(const WebCore::IntPoint point, WebCore::TextGranularity granularity, bool isInteractingWithFocusedElement, CompletionHandler<void(bool)>&& callback)
{
    sendWithAsyncReply(Messages::WebPage::UpdateSelectionWithExtentPointAndBoundary(point, granularity, isInteractingWithFocusedElement), WTFMove(callback));
}

void WebPageProxy::requestDictationContext(CompletionHandler<void(const String&, const String&, const String&)>&& callbackFunction)
{
    if (!hasRunningProcess())
        return callbackFunction({ }, { }, { });

    sendWithAsyncReply(Messages::WebPage::RequestDictationContext(), WTFMove(callbackFunction));
}

#if ENABLE(REVEAL)
void WebPageProxy::requestRVItemInCurrentSelectedRange(CompletionHandler<void(const WebKit::RevealItem&)>&& callbackFunction)
{
    if (!hasRunningProcess())
        return callbackFunction(RevealItem());

    sendWithAsyncReply(Messages::WebPage::RequestRVItemInCurrentSelectedRange(), WTFMove(callbackFunction));
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

        weakThis->sendWithAsyncReply(Messages::WebPage::PrepareSelectionForContextMenuWithLocationInView(point), WTFMove(callbackFunction));
    });
}
#endif

void WebPageProxy::requestAutocorrectionContext()
{
    m_process->send(Messages::WebPage::HandleAutocorrectionContextRequest(), m_webPageID);
}

void WebPageProxy::handleAutocorrectionContext(const WebAutocorrectionContext& context)
{
    pageClient().handleAutocorrectionContext(context);
}

void WebPageProxy::getSelectionContext(CompletionHandler<void(const String&, const String&, const String&)>&& callbackFunction)
{
    if (!hasRunningProcess())
        return callbackFunction({ }, { }, { });
    sendWithAsyncReply(Messages::WebPage::GetSelectionContext(), WTFMove(callbackFunction));
}

void WebPageProxy::handleTwoFingerTapAtPoint(const WebCore::IntPoint& point, OptionSet<WebEventModifier> modifiers, WebKit::TapIdentifier requestID)
{
    send(Messages::WebPage::HandleTwoFingerTapAtPoint(point, modifiers, requestID));
}

void WebPageProxy::selectWithTwoTouches(const WebCore::IntPoint from, const WebCore::IntPoint to, GestureType gestureType, GestureRecognizerState gestureState, CompletionHandler<void(const WebCore::IntPoint&, GestureType, GestureRecognizerState, OptionSet<SelectionFlags>)>&& callback)
{
    if (!hasRunningProcess())
        return callback({ }, GestureType::Loupe, GestureRecognizerState::Possible, { });

    sendWithAsyncReply(Messages::WebPage::SelectWithTwoTouches(from, to, gestureType, gestureState), WTFMove(callback));
}

void WebPageProxy::didReceivePositionInformation(const InteractionInformationAtPosition& info)
{
    pageClient().positionInformationDidChange(info);
}

void WebPageProxy::requestPositionInformation(const InteractionInformationRequest& request)
{
    m_process->send(Messages::WebPage::RequestPositionInformation(request), m_webPageID);
}

void WebPageProxy::startInteractionWithPositionInformation(const InteractionInformationAtPosition& positionInformation)
{
    m_process->send(Messages::WebPage::StartInteractionWithElementContextOrPosition(positionInformation.elementContext, positionInformation.request.point), m_webPageID);
}

void WebPageProxy::stopInteraction()
{
    m_process->send(Messages::WebPage::StopInteraction(), m_webPageID);
}

bool WebPageProxy::isValidPerformActionOnElementAuthorizationToken(const String& authorizationToken) const
{
    return !authorizationToken.isNull() && m_performActionOnElementAuthTokens.contains(authorizationToken);
}

void WebPageProxy::performActionOnElement(uint32_t action)
{
    auto authorizationToken = createVersion4UUIDString();

    m_performActionOnElementAuthTokens.add(authorizationToken);
    
    sendWithAsyncReply(Messages::WebPage::PerformActionOnElement(action, authorizationToken), [weakThis = WeakPtr { *this }, authorizationToken] () mutable {
        if (!weakThis)
            return;

        ASSERT(weakThis->isValidPerformActionOnElementAuthorizationToken(authorizationToken));
        weakThis->m_performActionOnElementAuthTokens.remove(authorizationToken);
    });
}

void WebPageProxy::saveImageToLibrary(const SharedMemory::Handle& imageHandle, const String& authorizationToken)
{
    MESSAGE_CHECK(!imageHandle.isNull());
    MESSAGE_CHECK(isValidPerformActionOnElementAuthorizationToken(authorizationToken));

    auto sharedMemoryBuffer = SharedMemory::map(imageHandle, SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryBuffer)
        return;

    auto buffer = sharedMemoryBuffer->createSharedBuffer(sharedMemoryBuffer->size());
    pageClient().saveImageToLibrary(WTFMove(buffer));
}

void WebPageProxy::applicationDidEnterBackground()
{
    m_lastObservedStateWasBackground = true;

    bool isSuspendedUnderLock = [UIApp isSuspendedUnderLock];
    
    WEBPAGEPROXY_RELEASE_LOG(ViewState, "applicationDidEnterBackground: isSuspendedUnderLock? %d", isSuspendedUnderLock);

#if !PLATFORM(WATCHOS)
    // We normally delay process suspension when the app is backgrounded until the current page load completes. However,
    // we do not want to do so when the screen is locked for power reasons.
    if (isSuspendedUnderLock)
        NavigationState::fromWebPage(*this).releaseNetworkActivity(NavigationState::NetworkActivityReleaseReason::ScreenLocked);
#endif
    m_process->send(Messages::WebPage::ApplicationDidEnterBackground(isSuspendedUnderLock), m_webPageID);
}

void WebPageProxy::applicationDidFinishSnapshottingAfterEnteringBackground()
{
    if (m_drawingArea) {
        m_drawingArea->prepareForAppSuspension();
        m_drawingArea->hideContentUntilPendingUpdate();
    }
    m_process->send(Messages::WebPage::ApplicationDidFinishSnapshottingAfterEnteringBackground(), m_webPageID);
}

bool WebPageProxy::isInHardwareKeyboardMode()
{
    return [UIKeyboard isInHardwareKeyboardMode];
}

void WebPageProxy::applicationWillEnterForeground()
{
    m_lastObservedStateWasBackground = false;

    bool isSuspendedUnderLock = [UIApp isSuspendedUnderLock];
    WEBPAGEPROXY_RELEASE_LOG(ViewState, "applicationWillEnterForeground: isSuspendedUnderLock? %d", isSuspendedUnderLock);

    m_process->send(Messages::WebPage::ApplicationWillEnterForeground(isSuspendedUnderLock), m_webPageID);
    m_process->send(Messages::WebPage::HardwareKeyboardAvailabilityChanged(isInHardwareKeyboardMode()), m_webPageID);
}

void WebPageProxy::applicationWillResignActive()
{
    m_process->send(Messages::WebPage::ApplicationWillResignActive(), m_webPageID);
}

void WebPageProxy::applicationDidEnterBackgroundForMedia()
{
    bool isSuspendedUnderLock = [UIApp isSuspendedUnderLock];
    WEBPAGEPROXY_RELEASE_LOG(ViewState, "applicationWillEnterForegroundForMedia: isSuspendedUnderLock? %d", isSuspendedUnderLock);

    m_process->send(Messages::WebPage::ApplicationDidEnterBackgroundForMedia(isSuspendedUnderLock), m_webPageID);
}

void WebPageProxy::applicationWillEnterForegroundForMedia()
{
    bool isSuspendedUnderLock = [UIApp isSuspendedUnderLock];
    WEBPAGEPROXY_RELEASE_LOG(ViewState, "applicationWillEnterForegroundForMedia: isSuspendedUnderLock? %d", isSuspendedUnderLock);

    m_process->send(Messages::WebPage::ApplicationWillEnterForegroundForMedia(isSuspendedUnderLock), m_webPageID);
}

void WebPageProxy::applicationDidBecomeActive()
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (m_videoFullscreenManager)
        m_videoFullscreenManager->applicationDidBecomeActive();
#endif
    m_process->send(Messages::WebPage::ApplicationDidBecomeActive(), m_webPageID);
}

void WebPageProxy::extendSelection(WebCore::TextGranularity granularity, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPage::ExtendSelection(granularity), [completionHandler = WTFMove(completionHandler)]() mutable {
        if (completionHandler)
            completionHandler();
    });
}

void WebPageProxy::selectWordBackward()
{
    m_process->send(Messages::WebPage::SelectWordBackward(), m_webPageID);
}

void WebPageProxy::extendSelectionForReplacement(CompletionHandler<void()>&& completion)
{
    sendWithAsyncReply(Messages::WebPage::ExtendSelectionForReplacement(), WTFMove(completion));
}

void WebPageProxy::requestRectsForGranularityWithSelectionOffset(WebCore::TextGranularity granularity, uint32_t offset, CompletionHandler<void(const Vector<WebCore::SelectionGeometry>&)>&& callback)
{
    if (!hasRunningProcess())
        return callback({ });
    
    sendWithAsyncReply(Messages::WebPage::GetRectsForGranularityWithSelectionOffset(granularity, offset), WTFMove(callback));
}

void WebPageProxy::requestRectsAtSelectionOffsetWithText(int32_t offset, const String& text, CompletionHandler<void(const Vector<WebCore::SelectionGeometry>&)>&& callback)
{
    if (!hasRunningProcess())
        return callback({ });
    
    sendWithAsyncReply(Messages::WebPage::GetRectsAtSelectionOffsetWithText(offset, text), WTFMove(callback));
}

void WebPageProxy::storeSelectionForAccessibility(bool shouldStore)
{
    m_process->send(Messages::WebPage::StoreSelectionForAccessibility(shouldStore), m_webPageID);
}

void WebPageProxy::startAutoscrollAtPosition(const WebCore::FloatPoint& positionInWindow)
{
    m_process->send(Messages::WebPage::StartAutoscrollAtPosition(positionInWindow), m_webPageID);
}
    
void WebPageProxy::cancelAutoscroll()
{
    m_process->send(Messages::WebPage::CancelAutoscroll(), m_webPageID);
}

void WebPageProxy::moveSelectionByOffset(int32_t offset, CompletionHandler<void()>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction();
        return;
    }
    
    sendWithAsyncReply(Messages::WebPage::MoveSelectionByOffset(offset), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = m_process->throttler().backgroundActivity("WebPageProxy::moveSelectionByOffset"_s)] () mutable {
        callbackFunction();
    });
}

void WebPageProxy::interpretKeyEvent(const EditorState& state, bool isCharEvent, CompletionHandler<void(bool)>&& completionHandler)
{
    updateEditorState(state);
    if (m_keyEventQueue.isEmpty())
        completionHandler(false);
    else
        completionHandler(pageClient().interpretKeyEvent(m_keyEventQueue.first(), isCharEvent));
}

void WebPageProxy::setSmartInsertDeleteEnabled(bool)
{
    notImplemented();
}

void WebPageProxy::registerWebProcessAccessibilityToken(const IPC::DataReference& data)
{
    pageClient().accessibilityWebProcessTokenReceived(data);
}    

void WebPageProxy::assistiveTechnologyMakeFirstResponder()
{
    pageClient().assistiveTechnologyMakeFirstResponder();
}
    
void WebPageProxy::makeFirstResponder()
{
    notImplemented();
}

void WebPageProxy::registerUIProcessAccessibilityTokens(const IPC::DataReference& elementToken, const IPC::DataReference& windowToken)
{
    if (!hasRunningProcess())
        return;
    
    send(Messages::WebPage::RegisterUIProcessAccessibilityTokens(elementToken, windowToken));
}

void WebPageProxy::executeSavedCommandBySelector(const String&, CompletionHandler<void(bool)>&& completionHandler)
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
    send(Messages::WebPage::WillStartUserTriggeredZooming());
}

void WebPageProxy::potentialTapAtPosition(const WebCore::FloatPoint& position, bool shouldRequestMagnificationInformation, WebKit::TapIdentifier requestID)
{
    hideValidationMessage();
    send(Messages::WebPage::PotentialTapAtPosition(requestID, position, shouldRequestMagnificationInformation));
}

void WebPageProxy::commitPotentialTap(OptionSet<WebEventModifier> modifiers, TransactionID layerTreeTransactionIdAtLastTouchStart, WebCore::PointerID pointerId)
{
    send(Messages::WebPage::CommitPotentialTap(modifiers, layerTreeTransactionIdAtLastTouchStart, pointerId));
}

void WebPageProxy::cancelPotentialTap()
{
    send(Messages::WebPage::CancelPotentialTap());
}

void WebPageProxy::tapHighlightAtPosition(const WebCore::FloatPoint& position, WebKit::TapIdentifier requestID)
{
    send(Messages::WebPage::TapHighlightAtPosition(requestID, position));
}

void WebPageProxy::attemptSyntheticClick(const FloatPoint& location, OptionSet<WebEventModifier> modifiers, TransactionID layerTreeTransactionIdAtLastTouchStart)
{
    send(Messages::WebPage::AttemptSyntheticClick(roundedIntPoint(location), modifiers, layerTreeTransactionIdAtLastTouchStart));
}

void WebPageProxy::didRecognizeLongPress()
{
    send(Messages::WebPage::DidRecognizeLongPress());
}

void WebPageProxy::handleDoubleTapForDoubleClickAtPoint(const WebCore::IntPoint& point, OptionSet<WebEventModifier> modifiers, TransactionID layerTreeTransactionIdAtLastTouchStart)
{
    send(Messages::WebPage::HandleDoubleTapForDoubleClickAtPoint(point, modifiers, layerTreeTransactionIdAtLastTouchStart));
}

void WebPageProxy::inspectorNodeSearchMovedToPosition(const WebCore::FloatPoint& position)
{
    send(Messages::WebPage::InspectorNodeSearchMovedToPosition(position));
}

void WebPageProxy::inspectorNodeSearchEndedAtPosition(const WebCore::FloatPoint& position)
{
    send(Messages::WebPage::InspectorNodeSearchEndedAtPosition(position));
}

void WebPageProxy::blurFocusedElement()
{
    send(Messages::WebPage::BlurFocusedElement());
}

FloatSize WebPageProxy::screenSize()
{
    return WebCore::screenSize();
}

FloatSize WebPageProxy::availableScreenSize()
{
    return WebCore::availableScreenSize();
}

FloatSize WebPageProxy::overrideScreenSize()
{
    return WebCore::overrideScreenSize();
}

float WebPageProxy::textAutosizingWidth()
{
    return WebCore::screenSize().width();
}

void WebPageProxy::couldNotRestorePageState()
{
    pageClient().couldNotRestorePageState();
}

void WebPageProxy::restorePageState(std::optional<WebCore::FloatPoint> scrollPosition, const WebCore::FloatPoint& scrollOrigin, const WebCore::FloatBoxExtent& obscuredInsetsOnSave, double scale)
{
    pageClient().restorePageState(scrollPosition, scrollOrigin, obscuredInsetsOnSave, scale);
}

void WebPageProxy::restorePageCenterAndScale(std::optional<WebCore::FloatPoint> center, double scale)
{
    pageClient().restorePageCenterAndScale(center, scale);
}

void WebPageProxy::didGetTapHighlightGeometries(WebKit::TapIdentifier requestID, const WebCore::Color& color, const Vector<WebCore::FloatQuad>& highlightedQuads, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius, bool nodeHasBuiltInClickHandling)
{
    pageClient().didGetTapHighlightGeometries(requestID, color, highlightedQuads, topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius, nodeHasBuiltInClickHandling);
}

void WebPageProxy::setIsShowingInputViewForFocusedElement(bool showingInputView)
{
    send(Messages::WebPage::SetIsShowingInputViewForFocusedElement(showingInputView));
}

void WebPageProxy::updateInputContextAfterBlurringAndRefocusingElement()
{
    pageClient().updateInputContextAfterBlurringAndRefocusingElement();
}

void WebPageProxy::elementDidFocus(const FocusedElementInformation& information, bool userIsInteracting, bool blurPreviousNode, OptionSet<WebCore::ActivityState::Flag> activityStateChanges, const UserData& userData)
{
    m_pendingInputModeChange = std::nullopt;

    API::Object* userDataObject = process().transformHandlesToObjects(userData.object()).get();
    pageClient().elementDidFocus(information, userIsInteracting, blurPreviousNode, activityStateChanges, userDataObject);
}

void WebPageProxy::elementDidBlur()
{
    m_pendingInputModeChange = std::nullopt;
    pageClient().elementDidBlur();
}

void WebPageProxy::focusedElementDidChangeInputMode(WebCore::InputMode mode)
{
#if ENABLE(TOUCH_EVENTS)
    if (m_touchAndPointerEventTracking.isTrackingAnything()) {
        m_pendingInputModeChange = mode;
        return;
    }
#endif

    pageClient().focusedElementDidChangeInputMode(mode);
}

void WebPageProxy::didReleaseAllTouchPoints()
{
    if (!m_pendingInputModeChange)
        return;

    pageClient().focusedElementDidChangeInputMode(*m_pendingInputModeChange);
    m_pendingInputModeChange = std::nullopt;
}

void WebPageProxy::autofillLoginCredentials(const String& username, const String& password)
{
    m_process->send(Messages::WebPage::AutofillLoginCredentials(username, password), m_webPageID);
}

void WebPageProxy::showInspectorHighlight(const WebCore::InspectorOverlay::Highlight& highlight)
{
    pageClient().showInspectorHighlight(highlight);
}

void WebPageProxy::hideInspectorHighlight()
{
    pageClient().hideInspectorHighlight();
}

void WebPageProxy::showInspectorIndication()
{
    pageClient().showInspectorIndication();
}

void WebPageProxy::hideInspectorIndication()
{
    pageClient().hideInspectorIndication();
}

void WebPageProxy::enableInspectorNodeSearch()
{
    pageClient().enableInspectorNodeSearch();
}

void WebPageProxy::disableInspectorNodeSearch()
{
    pageClient().disableInspectorNodeSearch();
}

void WebPageProxy::focusNextFocusedElement(bool isForward, CompletionHandler<void()>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction();
        return;
    }
    
    sendWithAsyncReply(Messages::WebPage::FocusNextFocusedElement(isForward), [callbackFunction = WTFMove(callbackFunction), backgroundActivity = m_process->throttler().backgroundActivity("WebPageProxy::focusNextFocusedElement"_s)] () mutable {
        callbackFunction();
    });
}

void WebPageProxy::setFocusedElementValue(const WebCore::ElementContext& context, const String& value)
{
    send(Messages::WebPage::SetFocusedElementValue(context, value));
}

void WebPageProxy::setFocusedElementSelectedIndex(const WebCore::ElementContext& context, uint32_t index, bool allowMultipleSelection)
{
    send(Messages::WebPage::SetFocusedElementSelectedIndex(context, index, allowMultipleSelection));
}

void WebPageProxy::didPerformDictionaryLookup(const DictionaryPopupInfo& dictionaryPopupInfo)
{
    pageClient().didPerformDictionaryLookup(dictionaryPopupInfo);
}

void WebPageProxy::setRemoteLayerTreeRootNode(RemoteLayerTreeNode* rootNode)
{
    pageClient().setRemoteLayerTreeRootNode(rootNode);
    m_frozenRemoteLayerTreeHost = nullptr;
}

void WebPageProxy::showPlaybackTargetPicker(bool hasVideo, const IntRect& elementRect, WebCore::RouteSharingPolicy policy, const String& contextUID)
{
    pageClient().showPlaybackTargetPicker(hasVideo, elementRect, policy, contextUID);
}

void WebPageProxy::commitPotentialTapFailed()
{
    pageClient().commitPotentialTapFailed();
}

void WebPageProxy::didNotHandleTapAsClick(const WebCore::IntPoint& point)
{
    pageClient().didNotHandleTapAsClick(point);
    m_uiClient->didNotHandleTapAsClick(point);
}

void WebPageProxy::didCompleteSyntheticClick()
{
    pageClient().didCompleteSyntheticClick();
}

void WebPageProxy::disableDoubleTapGesturesDuringTapIfNecessary(WebKit::TapIdentifier requestID)
{
    pageClient().disableDoubleTapGesturesDuringTapIfNecessary(requestID);
}

void WebPageProxy::handleSmartMagnificationInformationForPotentialTap(WebKit::TapIdentifier requestID, const WebCore::FloatRect& renderRect, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale, bool nodeIsRootLevel)
{
    pageClient().handleSmartMagnificationInformationForPotentialTap(requestID, renderRect, fitEntireRect, viewportMinimumScale, viewportMaximumScale, nodeIsRootLevel);
}

size_t WebPageProxy::computePagesForPrintingiOS(FrameIdentifier frameID, const PrintInfo& printInfo)
{
    ASSERT_WITH_MESSAGE(!printInfo.snapshotFirstPage, "If we are just snapshotting the first page, we don't need a synchronous message to determine the page count, which is 1.");

    if (!hasRunningProcess())
        return 0;

    auto sendResult = sendSync(Messages::WebPage::ComputePagesForPrintingiOS(frameID, printInfo), Seconds::infinity());
    auto [pageCount] = sendResult.takeReplyOr(0);
    return pageCount;
}

uint64_t WebPageProxy::drawToPDFiOS(FrameIdentifier frameID, const PrintInfo& printInfo, size_t pageCount, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler({ });
        return 0;
    }

    return sendWithAsyncReply(Messages::WebPage::DrawToPDFiOS(frameID, printInfo, pageCount), WTFMove(completionHandler));
}

void WebPageProxy::contentSizeCategoryDidChange(const String& contentSizeCategory)
{
    send(Messages::WebPage::ContentSizeCategoryDidChange(contentSizeCategory));
}

void WebPageProxy::generateSyntheticEditingCommand(WebKit::SyntheticEditingCommandType command)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::GenerateSyntheticEditingCommand(command));
}

void WebPageProxy::didUpdateEditorState(const EditorState& oldEditorState, const EditorState& newEditorState)
{
    bool couldChangeSecureInputState = newEditorState.isInPasswordField != oldEditorState.isInPasswordField || oldEditorState.selectionIsNone;
    
    // Selection being none is a temporary state when editing. Flipping secure input state too quickly was causing trouble (not fully understood).
    if (couldChangeSecureInputState && !newEditorState.selectionIsNone)
        pageClient().updateSecureInputState();
    
    if (newEditorState.shouldIgnoreSelectionChanges)
        return;
    
    updateFontAttributesAfterEditorStateChange();
    // We always need to notify the client on iOS to make sure the selection is redrawn,
    // even during composition to support phrase boundary gesture.
    pageClient().selectionDidChange();
}

void WebPageProxy::dispatchDidUpdateEditorState()
{
    if (!m_waitingForPostLayoutEditorStateUpdateAfterFocusingElement || !m_editorState.hasPostLayoutData())
        return;

    pageClient().didUpdateEditorState();
    m_waitingForPostLayoutEditorStateUpdateAfterFocusingElement = false;
}

void WebPageProxy::showValidationMessage(const IntRect& anchorClientRect, const String& message)
{
    m_validationBubble = pageClient().createValidationBubble(message, { m_preferences->minimumFontSize() });
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

void WebPageProxy::hardwareKeyboardAvailabilityChanged(bool keyboardIsAttached)
{
    updateCurrentModifierState();
    m_process->send(Messages::WebPage::HardwareKeyboardAvailabilityChanged(keyboardIsAttached), m_webPageID);
}

void WebPageProxy::requestEvasionRectsAboveSelection(CompletionHandler<void(const Vector<WebCore::FloatRect>&)>&& callback)
{
    if (!hasRunningProcess()) {
        callback({ });
        return;
    }

    sendWithAsyncReply(Messages::WebPage::RequestEvasionRectsAboveSelection(), WTFMove(callback));
}

void WebPageProxy::updateSelectionWithDelta(int64_t locationDelta, int64_t lengthDelta, CompletionHandler<void()>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler();
        return;
    }

    sendWithAsyncReply(Messages::WebPage::UpdateSelectionWithDelta(locationDelta, lengthDelta), WTFMove(completionHandler));
}

WebCore::FloatRect WebPageProxy::selectionBoundingRectInRootViewCoordinates() const
{
    if (m_editorState.selectionIsNone)
        return { };

    if (!m_editorState.hasVisualData())
        return { };

    WebCore::FloatRect bounds;
    auto& visualData = *m_editorState.visualData;
    if (m_editorState.selectionIsRange) {
        for (auto& geometry : visualData.selectionGeometries)
            bounds.unite(geometry.rect());
    } else
        bounds = visualData.caretRectAtStart;

    return bounds;
}

void WebPageProxy::requestDocumentEditingContext(WebKit::DocumentEditingContextRequest request, CompletionHandler<void(WebKit::DocumentEditingContext)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler({ });
        return;
    }

    sendWithAsyncReply(Messages::WebPage::RequestDocumentEditingContext(request), WTFMove(completionHandler));
}

#if ENABLE(DRAG_SUPPORT)

void WebPageProxy::didHandleDragStartRequest(bool started)
{
    pageClient().didHandleDragStartRequest(started);
}

void WebPageProxy::didHandleAdditionalDragItemsRequest(bool added)
{
    pageClient().didHandleAdditionalDragItemsRequest(added);
}

void WebPageProxy::requestDragStart(const WebCore::IntPoint& clientPosition, const WebCore::IntPoint& globalPosition, OptionSet<WebCore::DragSourceAction> allowedActionsMask)
{
    if (hasRunningProcess())
        m_process->send(Messages::WebPage::RequestDragStart(clientPosition, globalPosition, allowedActionsMask), m_webPageID);
}

void WebPageProxy::requestAdditionalItemsForDragSession(const IntPoint& clientPosition, const IntPoint& globalPosition, OptionSet<WebCore::DragSourceAction> allowedActionsMask)
{
    if (hasRunningProcess())
        m_process->send(Messages::WebPage::RequestAdditionalItemsForDragSession(clientPosition, globalPosition, allowedActionsMask), m_webPageID);
}

void WebPageProxy::insertDroppedImagePlaceholders(const Vector<IntSize>& imageSizes, CompletionHandler<void(const Vector<IntRect>&, std::optional<WebCore::TextIndicatorData>)>&& completionHandler)
{
    if (hasRunningProcess())
        sendWithAsyncReply(Messages::WebPage::InsertDroppedImagePlaceholders(imageSizes), WTFMove(completionHandler));
    else
        completionHandler({ }, std::nullopt);
}

void WebPageProxy::willReceiveEditDragSnapshot()
{
    pageClient().willReceiveEditDragSnapshot();
}

void WebPageProxy::didReceiveEditDragSnapshot(std::optional<TextIndicatorData> data)
{
    pageClient().didReceiveEditDragSnapshot(data);
}

void WebPageProxy::didConcludeDrop()
{
    m_process->send(Messages::WebPage::DidConcludeDrop(), m_webPageID);
}

#endif

#if USE(QUICK_LOOK)

void WebPageProxy::didStartLoadForQuickLookDocumentInMainFrame(const String& fileName, const String& uti)
{
    // Ensure that fileName isn't really a path name
    static_assert(notFound + 1 == 0, "The following line assumes WTF::notFound equals -1");
    m_navigationClient->didStartLoadForQuickLookDocumentInMainFrame(fileName.substring(fileName.reverseFind('/') + 1), uti);
}

void WebPageProxy::didFinishLoadForQuickLookDocumentInMainFrame(const ShareableResource::Handle& handle)
{
    auto buffer = handle.tryWrapInSharedBuffer();
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
    pageClient().requestPasswordForQuickLookDocument(fileName, WTFMove(completionHandler));
}

#endif

#if ENABLE(APPLE_PAY)

std::unique_ptr<PaymentAuthorizationPresenter> WebPageProxy::paymentCoordinatorAuthorizationPresenter(WebPaymentCoordinatorProxy& paymentCoordinatorProxy, PKPaymentRequest *paymentRequest)
{
    return makeUnique<PaymentAuthorizationViewController>(paymentCoordinatorProxy, paymentRequest);
}

UIViewController *WebPageProxy::paymentCoordinatorPresentingViewController(const WebPaymentCoordinatorProxy&)
{
    return uiClient().presentingViewController();
}

#if ENABLE(APPLE_PAY_REMOTE_UI_USES_SCENE)
void WebPageProxy::getWindowSceneIdentifierForPaymentPresentation(WebPageProxyIdentifier, CompletionHandler<void(const String&)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(nullString());
}
#endif

const String& WebPageProxy::paymentCoordinatorCTDataConnectionServiceType(const WebPaymentCoordinatorProxy&)
{
    return websiteDataStore().configuration().dataConnectionServiceType();
}

#endif // ENABLE(APPLE_PAY)

static bool desktopClassBrowsingSupported()
{
    static bool supportsDesktopClassBrowsing = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        supportsDesktopClassBrowsing = !currentUserInterfaceIdiomIsSmallScreen();
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
    // FIXME: This should be additionally gated on site-specific quirks being enabled. However, site-specific quirks are already
    // disabled by default in WKWebView, so we would need a new preference for controlling site-specific quirks that are on-by-default
    // in all apps, but may be turned off via SPI (or via Web Inspector). See also: <rdar://problem/50035167>.
    auto host = request.url().host();
    if (equalLettersIgnoringASCIICase(host, "tv.kakao.com"_s) || host.endsWithIgnoringASCIICase(".tv.kakao.com"_s))
        return RecommendDesktopClassBrowsingForRequest::No;

    if (equalLettersIgnoringASCIICase(host, "tving.com"_s) || host.endsWithIgnoringASCIICase(".tving.com"_s))
        return RecommendDesktopClassBrowsingForRequest::No;

    if (equalLettersIgnoringASCIICase(host, "live.iqiyi.com"_s) || host.endsWithIgnoringASCIICase(".live.iqiyi.com"_s))
        return RecommendDesktopClassBrowsingForRequest::No;

    if (equalLettersIgnoringASCIICase(host, "jsfiddle.net"_s) || host.endsWithIgnoringASCIICase(".jsfiddle.net"_s))
        return RecommendDesktopClassBrowsingForRequest::No;

    if (equalLettersIgnoringASCIICase(host, "video.sina.com.cn"_s) || host.endsWithIgnoringASCIICase(".video.sina.com.cn"_s))
        return RecommendDesktopClassBrowsingForRequest::No;

    if (equalLettersIgnoringASCIICase(host, "huya.com"_s) || host.endsWithIgnoringASCIICase(".huya.com"_s))
        return RecommendDesktopClassBrowsingForRequest::No;

    if (equalLettersIgnoringASCIICase(host, "video.tudou.com"_s) || host.endsWithIgnoringASCIICase(".video.tudou.com"_s))
        return RecommendDesktopClassBrowsingForRequest::No;

    if (equalLettersIgnoringASCIICase(host, "cctv.com"_s) || host.endsWithIgnoringASCIICase(".cctv.com"_s))
        return RecommendDesktopClassBrowsingForRequest::No;

    if (equalLettersIgnoringASCIICase(host, "v.china.com.cn"_s))
        return RecommendDesktopClassBrowsingForRequest::No;

    if (equalLettersIgnoringASCIICase(host, "trello.com"_s) || host.endsWithIgnoringASCIICase(".trello.com"_s))
        return RecommendDesktopClassBrowsingForRequest::No;

    if (equalLettersIgnoringASCIICase(host, "ted.com"_s) || host.endsWithIgnoringASCIICase(".ted.com"_s))
        return RecommendDesktopClassBrowsingForRequest::No;

    if (host.containsIgnoringASCIICase("hsbc."_s)) {
        if (equalLettersIgnoringASCIICase(host, "hsbc.com.au"_s) || host.endsWithIgnoringASCIICase(".hsbc.com.au"_s))
            return RecommendDesktopClassBrowsingForRequest::No;
        if (equalLettersIgnoringASCIICase(host, "hsbc.com.eg"_s) || host.endsWithIgnoringASCIICase(".hsbc.com.eg"_s))
            return RecommendDesktopClassBrowsingForRequest::No;
        if (equalLettersIgnoringASCIICase(host, "hsbc.lk"_s) || host.endsWithIgnoringASCIICase(".hsbc.lk"_s))
            return RecommendDesktopClassBrowsingForRequest::No;
        if (equalLettersIgnoringASCIICase(host, "hsbc.co.uk"_s) || host.endsWithIgnoringASCIICase(".hsbc.co.uk"_s))
            return RecommendDesktopClassBrowsingForRequest::No;
        if (equalLettersIgnoringASCIICase(host, "hsbc.com.hk"_s) || host.endsWithIgnoringASCIICase(".hsbc.com.hk"_s))
            return RecommendDesktopClassBrowsingForRequest::No;
        if (equalLettersIgnoringASCIICase(host, "hsbc.com.mx"_s) || host.endsWithIgnoringASCIICase(".hsbc.com.mx"_s))
            return RecommendDesktopClassBrowsingForRequest::No;
        if (equalLettersIgnoringASCIICase(host, "hsbc.ca"_s) || host.endsWithIgnoringASCIICase(".hsbc.ca"_s))
            return RecommendDesktopClassBrowsingForRequest::No;
        if (equalLettersIgnoringASCIICase(host, "hsbc.com.ar"_s) || host.endsWithIgnoringASCIICase(".hsbc.com.ar"_s))
            return RecommendDesktopClassBrowsingForRequest::No;
        if (equalLettersIgnoringASCIICase(host, "hsbc.com.ph"_s) || host.endsWithIgnoringASCIICase(".hsbc.com.ph"_s))
            return RecommendDesktopClassBrowsingForRequest::No;
        if (equalLettersIgnoringASCIICase(host, "hsbc.com"_s) || host.endsWithIgnoringASCIICase(".hsbc.com"_s))
            return RecommendDesktopClassBrowsingForRequest::No;
        if (equalLettersIgnoringASCIICase(host, "hsbc.com.cn"_s) || host.endsWithIgnoringASCIICase(".hsbc.com.cn"_s))
            return RecommendDesktopClassBrowsingForRequest::No;
    }

    if (equalLettersIgnoringASCIICase(host, "nhl.com"_s) || host.endsWithIgnoringASCIICase(".nhl.com"_s))
        return RecommendDesktopClassBrowsingForRequest::No;

    // FIXME: Remove this quirk when <rdar://problem/59480381> is complete.
    if (equalLettersIgnoringASCIICase(host, "fidelity.com"_s) || host.endsWithIgnoringASCIICase(".fidelity.com"_s))
        return RecommendDesktopClassBrowsingForRequest::No;

    // FIXME: Remove this quirk when <rdar://problem/61733101> is complete.
    if (equalLettersIgnoringASCIICase(host, "roblox.com"_s) || host.endsWithIgnoringASCIICase(".roblox.com"_s))
        return RecommendDesktopClassBrowsingForRequest::No;

    return RecommendDesktopClassBrowsingForRequest::Auto;
}

bool WebPageProxy::isDesktopClassBrowsingRecommended(const WebCore::ResourceRequest& request) const
{
    auto desktopClassBrowsingRecommendation = desktopClassBrowsingRecommendedForRequest(request);
    if (desktopClassBrowsingRecommendation == RecommendDesktopClassBrowsingForRequest::Yes)
        return true;

    if (desktopClassBrowsingRecommendation == RecommendDesktopClassBrowsingForRequest::No)
        return false;

#if !PLATFORM(MACCATALYST)
    if (!pageClient().hasResizableWindows() && webViewSizeIsNarrow(viewSize()))
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
    if (!m_configuration->defaultWebsitePolicies())
        return userAgent();

    const API::WebsitePolicies& policies = *m_configuration->defaultWebsitePolicies();
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

    bool useDesktopBrowsingMode = useDesktopClassBrowsing(policies, request);

    m_preferFasterClickOverDoubleTap = false;

    if (!useDesktopBrowsingMode) {
        policies.setAllowContentChangeObserverQuirk(true);
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

    return WebContentMode::Desktop;
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

    bool canTakeForegroundAssertions = pageClient().canTakeForegroundAssertions();
    WEBPAGEPROXY_RELEASE_LOG(Process, "WebPageProxy::shouldForceForegroundPriorityForClientNavigation() returns %d based on PageClient::canTakeForegroundAssertions()", canTakeForegroundAssertions);
    return canTakeForegroundAssertions;
}

void WebPageProxy::textInputContextsInRect(FloatRect rect, CompletionHandler<void(const Vector<ElementContext>&)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler({ });
        return;
    }

    sendWithAsyncReply(Messages::WebPage::TextInputContextsInRect(rect), WTFMove(completionHandler));
}

void WebPageProxy::focusTextInputContextAndPlaceCaret(const ElementContext& context, const IntPoint& point, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler(false);
        return;
    }

    sendWithAsyncReply(Messages::WebPage::FocusTextInputContextAndPlaceCaret(context, point), WTFMove(completionHandler));
}

void WebPageProxy::setShouldRevealCurrentSelectionAfterInsertion(bool shouldRevealCurrentSelectionAfterInsertion)
{
    if (hasRunningProcess())
        send(Messages::WebPage::SetShouldRevealCurrentSelectionAfterInsertion(shouldRevealCurrentSelectionAfterInsertion));
}

void WebPageProxy::setScreenIsBeingCaptured(bool captured)
{
    if (hasRunningProcess())
        send(Messages::WebPage::SetScreenIsBeingCaptured(captured));
}

void WebPageProxy::willOpenAppLink()
{
    if (m_openingAppLinkActivity && m_openingAppLinkActivity->isValid())
        return;

    // We take a background activity for 25 seconds when switching to another app via an app link in case the WebPage
    // needs to run script to pass information to the native app.
    // We chose 25 seconds because the system only gives us 30 seconds and we don't want to get too close to that limit
    // to avoid assertion invalidation (or even termination).
    m_openingAppLinkActivity = process().throttler().backgroundActivity("Opening AppLink"_s).moveToUniquePtr();
    WorkQueue::main().dispatchAfter(25_s, [weakThis = WeakPtr { *this }] {
        if (weakThis)
            weakThis->m_openingAppLinkActivity = nullptr;
    });
}

void WebPageProxy::processWillBecomeSuspended()
{
    if (!hasRunningProcess())
        return;

    m_hasNetworkRequestsOnSuspended = m_pageLoadState.networkRequestsInProgress();
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
void WebPageProxy::isUserFacingChanged(bool isUserFacing)
{
    if (!isUserFacing)
        suspendAllMediaPlayback([] { });
    else
        resumeAllMediaPlayback([] { });
}
#endif

void WebPageProxy::willPerformPasteCommand(DOMPasteAccessCategory pasteAccessCategory)
{
    switch (pasteAccessCategory) {
    case DOMPasteAccessCategory::General:
    case DOMPasteAccessCategory::Fonts:
        grantAccessToCurrentPasteboardData(UIPasteboardNameGeneral);
        return;
    }
}

void WebPageProxy::setDeviceHasAGXCompilerServiceForTesting() const
{
    WebCore::setDeviceHasAGXCompilerServiceForTesting();
}

void WebPageProxy::showDataDetectorsUIForPositionInformation(const InteractionInformationAtPosition& positionInfo)
{
    pageClient().showDataDetectorsUIForPositionInformation(positionInfo);
}

Color WebPageProxy::platformUnderPageBackgroundColor() const
{
    if (auto contentViewBackgroundColor = pageClient().contentViewBackgroundColor(); contentViewBackgroundColor.isValid())
        return contentViewBackgroundColor;

    return WebCore::Color::white;
}

void WebPageProxy::statusBarWasTapped()
{
#if PLATFORM(IOS)
    RELEASE_LOG_INFO(WebRTC, "WebPageProxy::statusBarWasTapped");

#if USE(APPLE_INTERNAL_SDK)
    UIApplication *app = UIApplication.sharedApplication;
    if (!app.supportsMultipleScenes && app.applicationState != UIApplicationStateActive)
        [[LSApplicationWorkspace defaultWorkspace] openApplicationWithBundleID:[[NSBundle mainBundle] bundleIdentifier]];
#endif

    m_uiClient->statusBarWasTapped();
#endif
}

} // namespace WebKit

#undef WEBPAGEPROXY_RELEASE_LOG
#undef MESSAGE_CHECK

#endif // PLATFORM(IOS_FAMILY)
