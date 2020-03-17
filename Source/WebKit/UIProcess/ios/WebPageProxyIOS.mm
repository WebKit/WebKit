/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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
#import "ShareableResource.h"
#import "UserData.h"
#import "UserInterfaceIdiom.h"
#import "VersionChecks.h"
#import "VideoFullscreenManagerProxy.h"
#import "ViewUpdateDispatcherMessages.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WebAutocorrectionContext.h"
#import "WebAutocorrectionData.h"
#import "WebPageMessages.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import <WebCore/FrameView.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/UserAgent.h>
#import <WebCore/ValidationBubble.h>
#import <pal/spi/ios/MobileGestaltSPI.h>
#import <wtf/text/TextStream.h>

#if USE(QUICK_LOOK)
#import "APILoaderClient.h"
#import "APINavigationClient.h"
#import <wtf/text/WTFString.h>
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, process().connection())

#define RELEASE_LOG_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), channel, "%p - [pageProxyID=%llu, webPageID=%llu, PID=%i] WebPageProxy::" fmt, this, m_identifier.toUInt64(), m_webPageID.toUInt64(), m_process->processIdentifier(), ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

void WebPageProxy::platformInitialize()
{
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

void WebPageProxy::gestureCallback(const WebCore::IntPoint& point, uint32_t gestureType, uint32_t gestureState, uint32_t flags, CallbackID callbackID)
{
    auto callback = m_callbacks.take<GestureCallback>(callbackID);
    if (!callback) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    callback->performCallbackWithReturnValue(point, gestureType, gestureState, flags);
}

void WebPageProxy::touchesCallback(const WebCore::IntPoint& point, uint32_t touches, uint32_t flags, CallbackID callbackID)
{
    auto callback = m_callbacks.take<TouchesCallback>(callbackID);
    if (!callback) {
        ASSERT_NOT_REACHED();
        return;
    }

    callback->performCallbackWithReturnValue(point, touches, flags);
}

void WebPageProxy::selectionContextCallback(const String& selectedText, const String& beforeText, const String& afterText, CallbackID callbackID)
{
    auto callback = m_callbacks.take<SelectionContextCallback>(callbackID);
    if (!callback) {
        ASSERT_NOT_REACHED();
        return;
    }

    callback->performCallbackWithReturnValue(selectedText, beforeText, afterText);
}

void WebPageProxy::selectionRectsCallback(const Vector<WebCore::SelectionRect>& selectionRects, CallbackID callbackID)
{
    auto callback = m_callbacks.take<SelectionRectsCallback>(callbackID);
    if (!callback) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    callback->performCallbackWithReturnValue(selectionRects);
}

void WebPageProxy::focusedElementInformationCallback(const FocusedElementInformation& info, CallbackID callbackID)
{
    auto callback = m_callbacks.take<FocusedElementInformationCallback>(callbackID);
    if (!callback) {
        ASSERT_NOT_REACHED();
        return;
    }

    callback->performCallbackWithReturnValue(info);
}

void WebPageProxy::requestFocusedElementInformation(Function<void(const FocusedElementInformation&, CallbackBase::Error)>&& callback)
{
    if (!hasRunningProcess()) {
        callback({ }, CallbackBase::Error::OwnerWasInvalidated);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callback), m_process->throttler().backgroundActivity("WebPageProxy::requestFocusedElementInformation"_s));
    m_process->send(Messages::WebPage::RequestFocusedElementInformation(callbackID), m_webPageID);
}

void WebPageProxy::updateVisibleContentRects(const VisibleContentRectUpdateInfo& visibleContentRectUpdate)
{
    if (visibleContentRectUpdate == m_lastVisibleContentRectUpdate)
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

void WebPageProxy::scrollingNodeScrollViewWillStartPanGesture()
{
    pageClient().scrollingNodeScrollViewWillStartPanGesture();
}

void WebPageProxy::scrollingNodeScrollViewDidScroll()
{
    pageClient().scrollingNodeScrollViewDidScroll();
}

void WebPageProxy::scrollingNodeScrollWillStartScroll()
{
    pageClient().scrollingNodeScrollWillStartScroll();
}

void WebPageProxy::scrollingNodeScrollDidEndScroll()
{
    pageClient().scrollingNodeScrollDidEndScroll();
}

void WebPageProxy::dynamicViewportSizeUpdate(const FloatSize& viewLayoutSize, const WebCore::FloatSize& maximumUnobscuredSize, const FloatRect& targetExposedContentRect, const FloatRect& targetUnobscuredRect, const FloatRect& targetUnobscuredRectInScrollViewCoordinates, const WebCore::FloatBoxExtent& unobscuredSafeAreaInsets, double targetScale, int32_t deviceOrientation, double minimumEffectiveDeviceWidth, DynamicViewportSizeUpdateID dynamicViewportSizeUpdateID)
{
    if (!hasRunningProcess())
        return;

    hideValidationMessage();

    m_viewportConfigurationViewLayoutSize = viewLayoutSize;
    m_process->send(Messages::WebPage::DynamicViewportSizeUpdate(viewLayoutSize,
        maximumUnobscuredSize, targetExposedContentRect, targetUnobscuredRect,
        targetUnobscuredRectInScrollViewCoordinates, unobscuredSafeAreaInsets,
        targetScale, deviceOrientation, minimumEffectiveDeviceWidth, dynamicViewportSizeUpdateID), m_webPageID);
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

void WebPageProxy::setMaximumUnobscuredSize(const WebCore::FloatSize& size)
{
    m_maximumUnobscuredSize = size;

    if (hasRunningProcess())
        m_process->send(Messages::WebPage::SetMaximumUnobscuredSize(size), m_webPageID);
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

void WebPageProxy::setOverrideViewportArguments(const Optional<ViewportArguments>& viewportArguments)
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
    m_pageExtendedBackgroundColor = layerTreeTransaction.pageExtendedBackgroundColor();

    if (!m_hasReceivedLayerTreeTransactionAfterDidCommitLoad) {
        if (layerTreeTransaction.transactionID() >= m_firstLayerTreeTransactionIdAfterDidCommitLoad) {
            m_hasReceivedLayerTreeTransactionAfterDidCommitLoad = true;
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

    if (auto arguments = std::exchange(m_deferredElementDidFocusArguments, nullptr))
        pageClient().elementDidFocus(arguments->information, arguments->userIsInteracting, arguments->blurPreviousNode, arguments->activityStateChanges, arguments->userData.get());
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

void WebPageProxy::selectWithGesture(const WebCore::IntPoint point, WebCore::TextGranularity granularity, uint32_t gestureType, uint32_t gestureState, bool isInteractingWithFocusedElement, WTF::Function<void(const WebCore::IntPoint&, uint32_t, uint32_t, uint32_t, CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(WebCore::IntPoint(), 0, 0, 0, CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::selectWithGesture"_s));
    m_process->send(Messages::WebPage::SelectWithGesture(point, (uint32_t)granularity, gestureType, gestureState, isInteractingWithFocusedElement, callbackID), m_webPageID);
}

void WebPageProxy::updateSelectionWithTouches(const WebCore::IntPoint point, uint32_t touches, bool baseIsStart, WTF::Function<void (const WebCore::IntPoint&, uint32_t, uint32_t, CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(WebCore::IntPoint(), 0, 0, CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::updateSelectionWithTouches"_s));
    m_process->send(Messages::WebPage::UpdateSelectionWithTouches(point, touches, baseIsStart, callbackID), m_webPageID);
}
    
void WebPageProxy::replaceDictatedText(const String& oldText, const String& newText)
{
    m_process->send(Messages::WebPage::ReplaceDictatedText(oldText, newText), m_webPageID);
}

void WebPageProxy::replaceSelectedText(const String& oldText, const String& newText)
{
    m_process->send(Messages::WebPage::ReplaceSelectedText(oldText, newText), m_webPageID);
}

void WebPageProxy::requestAutocorrectionData(const String& textForAutocorrection, CompletionHandler<void(WebAutocorrectionData)>&& callback)
{
    if (!hasRunningProcess()) {
        callback({ });
        return;
    }
    m_process->connection()->sendWithAsyncReply(Messages::WebPage::RequestAutocorrectionData(textForAutocorrection), WTFMove(callback), m_webPageID);
}

void WebPageProxy::applyAutocorrection(const String& correction, const String& originalText, WTF::Function<void (const String&, CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(String(), CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::applyAutocorrection"_s));
    m_process->send(Messages::WebPage::ApplyAutocorrection(correction, originalText, callbackID), m_webPageID);
}

bool WebPageProxy::applyAutocorrection(const String& correction, const String& originalText)
{
    bool autocorrectionApplied = false;
    m_process->sendSync(Messages::WebPage::SyncApplyAutocorrection(correction, originalText), Messages::WebPage::SyncApplyAutocorrection::Reply(autocorrectionApplied), m_webPageID);
    return autocorrectionApplied;
}

void WebPageProxy::selectTextWithGranularityAtPoint(const WebCore::IntPoint point, WebCore::TextGranularity granularity, bool isInteractingWithFocusedElement, WTF::Function<void(CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::selectTextWithGranularityAtPoint"_s));
    m_process->send(Messages::WebPage::SelectTextWithGranularityAtPoint(point, static_cast<uint32_t>(granularity), isInteractingWithFocusedElement, callbackID), m_webPageID);
}

void WebPageProxy::selectPositionAtBoundaryWithDirection(const WebCore::IntPoint point, WebCore::TextGranularity granularity, WebCore::SelectionDirection direction, bool isInteractingWithFocusedElement, WTF::Function<void(CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::selectPositionAtBoundaryWithDirection"_s));
    m_process->send(Messages::WebPage::SelectPositionAtBoundaryWithDirection(point, static_cast<uint32_t>(granularity), static_cast<uint32_t>(direction), isInteractingWithFocusedElement, callbackID), m_webPageID);
}

void WebPageProxy::moveSelectionAtBoundaryWithDirection(WebCore::TextGranularity granularity, WebCore::SelectionDirection direction, WTF::Function<void(CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::moveSelectionAtBoundaryWithDirection"_s));
    m_process->send(Messages::WebPage::MoveSelectionAtBoundaryWithDirection(static_cast<uint32_t>(granularity), static_cast<uint32_t>(direction), callbackID), m_webPageID);
}
    
void WebPageProxy::selectPositionAtPoint(const WebCore::IntPoint point, bool isInteractingWithFocusedElement, WTF::Function<void(CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::selectPositionAtPoint"_s));
    m_process->send(Messages::WebPage::SelectPositionAtPoint(point, isInteractingWithFocusedElement, callbackID), m_webPageID);
}

void WebPageProxy::beginSelectionInDirection(WebCore::SelectionDirection direction, WTF::Function<void (uint64_t, CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(0, CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::beginSelectionInDirection"_s));
    m_process->send(Messages::WebPage::BeginSelectionInDirection(direction, callbackID), m_webPageID);
}

void WebPageProxy::updateSelectionWithExtentPoint(const WebCore::IntPoint point, bool isInteractingWithFocusedElement, RespectSelectionAnchor respectSelectionAnchor, WTF::Function<void(uint64_t, CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(0, CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::updateSelectionWithExtentPoint"_s));
    m_process->send(Messages::WebPage::UpdateSelectionWithExtentPoint(point, isInteractingWithFocusedElement, respectSelectionAnchor, callbackID), m_webPageID);
    
}

void WebPageProxy::updateSelectionWithExtentPointAndBoundary(const WebCore::IntPoint point, WebCore::TextGranularity granularity, bool isInteractingWithFocusedElement, WTF::Function<void(uint64_t, CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(0, CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::updateSelectionWithExtentPointAndBoundary"_s));
    m_process->send(Messages::WebPage::UpdateSelectionWithExtentPointAndBoundary(point, granularity, isInteractingWithFocusedElement, callbackID), m_webPageID);
    
}

void WebPageProxy::requestDictationContext(WTF::Function<void (const String&, const String&, const String&, CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(String(), String(), String(), CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::requestDictationContext"_s));
    m_process->send(Messages::WebPage::RequestDictationContext(callbackID), m_webPageID);
}

void WebPageProxy::requestAutocorrectionContext()
{
    m_process->send(Messages::WebPage::RequestAutocorrectionContext(), m_webPageID);
}

void WebPageProxy::handleAutocorrectionContext(const WebAutocorrectionContext& context)
{
    pageClient().handleAutocorrectionContext(context);
}

void WebPageProxy::getSelectionContext(WTF::Function<void(const String&, const String&, const String&, CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(String(), String(), String(), CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::getSelectionContext"_s));
    m_process->send(Messages::WebPage::GetSelectionContext(callbackID), m_webPageID);
}

void WebPageProxy::handleTwoFingerTapAtPoint(const WebCore::IntPoint& point, OptionSet<WebEvent::Modifier> modifiers, uint64_t requestID)
{
    process().send(Messages::WebPage::HandleTwoFingerTapAtPoint(point, modifiers, requestID), m_webPageID);
}

void WebPageProxy::handleStylusSingleTapAtPoint(const WebCore::IntPoint& point, uint64_t requestID)
{
    process().send(Messages::WebPage::HandleStylusSingleTapAtPoint(point, requestID), m_webPageID);
}

void WebPageProxy::selectWithTwoTouches(const WebCore::IntPoint from, const WebCore::IntPoint to, uint32_t gestureType, uint32_t gestureState, WTF::Function<void (const WebCore::IntPoint&, uint32_t, uint32_t, uint32_t, CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(WebCore::IntPoint(), 0, 0, 0, CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::selectWithTwoTouches"_s));
    m_process->send(Messages::WebPage::SelectWithTwoTouches(from, to, gestureType, gestureState, callbackID), m_webPageID);
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

void WebPageProxy::performActionOnElement(uint32_t action)
{
    m_process->send(Messages::WebPage::PerformActionOnElement(action), m_webPageID);
}

void WebPageProxy::saveImageToLibrary(const SharedMemory::Handle& imageHandle, uint64_t imageSize)
{
    MESSAGE_CHECK(!imageHandle.isNull());
    // SharedMemory::Handle::size() is rounded up to the nearest page.
    MESSAGE_CHECK(imageSize && imageSize <= imageHandle.size());

    auto sharedMemoryBuffer = SharedMemory::map(imageHandle, SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryBuffer)
        return;

    auto buffer = SharedBuffer::create(static_cast<unsigned char*>(sharedMemoryBuffer->data()), static_cast<size_t>(imageSize));
    pageClient().saveImageToLibrary(WTFMove(buffer));
}

void WebPageProxy::applicationDidEnterBackground()
{
    m_lastObservedStateWasBackground = true;

    bool isSuspendedUnderLock = [UIApp isSuspendedUnderLock];
    
    RELEASE_LOG_IF_ALLOWED(ViewState, "applicationDidEnterBackground: isSuspendedUnderLock? %d", isSuspendedUnderLock);

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
    RELEASE_LOG_IF_ALLOWED(ViewState, "applicationWillEnterForeground: isSuspendedUnderLock? %d", isSuspendedUnderLock);

    m_process->send(Messages::WebPage::ApplicationWillEnterForeground(isSuspendedUnderLock), m_webPageID);
    m_process->send(Messages::WebPage::HardwareKeyboardAvailabilityChanged(isInHardwareKeyboardMode()), m_webPageID);
}

void WebPageProxy::applicationWillResignActive()
{
    m_process->send(Messages::WebPage::ApplicationWillResignActive(), m_webPageID);
}

void WebPageProxy::applicationDidBecomeActive()
{
#if HAVE(AVKIT)
    if (m_videoFullscreenManager)
        m_videoFullscreenManager->applicationDidBecomeActive();
#endif
    m_process->send(Messages::WebPage::ApplicationDidBecomeActive(), m_webPageID);
}

void WebPageProxy::extendSelection(WebCore::TextGranularity granularity)
{
    m_process->send(Messages::WebPage::ExtendSelection(static_cast<uint32_t>(granularity)), m_webPageID);
}

void WebPageProxy::selectWordBackward()
{
    m_process->send(Messages::WebPage::SelectWordBackward(), m_webPageID);
}

void WebPageProxy::requestRectsForGranularityWithSelectionOffset(WebCore::TextGranularity granularity, uint32_t offset, WTF::Function<void(const Vector<WebCore::SelectionRect>&, CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(Vector<WebCore::SelectionRect>(), CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::requestRectsForGranularityWithSelectionOffset"_s));
    m_process->send(Messages::WebPage::GetRectsForGranularityWithSelectionOffset(static_cast<uint32_t>(granularity), offset, callbackID), m_webPageID);
}

void WebPageProxy::requestRectsAtSelectionOffsetWithText(int32_t offset, const String& text, WTF::Function<void(const Vector<WebCore::SelectionRect>&, CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(Vector<WebCore::SelectionRect>(), CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::requestRectsAtSelectionOffsetWithText"_s));
    m_process->send(Messages::WebPage::GetRectsAtSelectionOffsetWithText(offset, text, callbackID), m_webPageID);
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

void WebPageProxy::moveSelectionByOffset(int32_t offset, WTF::Function<void (CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::moveSelectionByOffset"_s));
    m_process->send(Messages::WebPage::MoveSelectionByOffset(offset, callbackID), m_webPageID);
}

void WebPageProxy::interpretKeyEvent(const EditorState& state, bool isCharEvent, CompletionHandler<void(bool)>&& completionHandler)
{
    m_editorState = state;
    completionHandler(pageClient().interpretKeyEvent(m_keyEventQueue.first(), isCharEvent));
}

// Complex text input support for plug-ins.
void WebPageProxy::sendComplexTextInputToPlugin(uint64_t, const String&)
{
    notImplemented();
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
    notImplemented();
}
    
void WebPageProxy::makeFirstResponder()
{
    notImplemented();
}

void WebPageProxy::registerUIProcessAccessibilityTokens(const IPC::DataReference& elementToken, const IPC::DataReference& windowToken)
{
    if (!hasRunningProcess())
        return;
    
    process().send(Messages::WebPage::RegisterUIProcessAccessibilityTokens(elementToken, windowToken), m_webPageID);
}

void WebPageProxy::pluginFocusOrWindowFocusChanged(uint64_t, bool)
{
    notImplemented();
}

void WebPageProxy::setPluginComplexTextInputState(uint64_t, uint64_t)
{
    notImplemented();
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

bool WebPageProxy::acceptsFirstMouse(int, const WebKit::WebMouseEvent&)
{
    notImplemented();
    return false;
}

void WebPageProxy::willStartUserTriggeredZooming()
{
    process().send(Messages::WebPage::WillStartUserTriggeredZooming(), m_webPageID);
}

void WebPageProxy::potentialTapAtPosition(const WebCore::FloatPoint& position, bool shouldRequestMagnificationInformation, uint64_t& requestID)
{
    hideValidationMessage();
    process().send(Messages::WebPage::PotentialTapAtPosition(requestID, position, shouldRequestMagnificationInformation), m_webPageID);
}

void WebPageProxy::commitPotentialTap(OptionSet<WebEvent::Modifier> modifiers, TransactionID layerTreeTransactionIdAtLastTouchStart, WebCore::PointerID pointerId)
{
    process().send(Messages::WebPage::CommitPotentialTap(modifiers, layerTreeTransactionIdAtLastTouchStart, pointerId), m_webPageID);
}

void WebPageProxy::cancelPotentialTap()
{
    process().send(Messages::WebPage::CancelPotentialTap(), m_webPageID);
}

void WebPageProxy::tapHighlightAtPosition(const WebCore::FloatPoint& position, uint64_t& requestID)
{
    process().send(Messages::WebPage::TapHighlightAtPosition(requestID, position), m_webPageID);
}

void WebPageProxy::handleTap(const FloatPoint& location, OptionSet<WebEvent::Modifier> modifiers, TransactionID layerTreeTransactionIdAtLastTouchStart)
{
    process().send(Messages::WebPage::HandleTap(roundedIntPoint(location), modifiers, layerTreeTransactionIdAtLastTouchStart), m_webPageID);
}

void WebPageProxy::didRecognizeLongPress()
{
    process().send(Messages::WebPage::DidRecognizeLongPress(), m_webPageID);
}

void WebPageProxy::handleDoubleTapForDoubleClickAtPoint(const WebCore::IntPoint& point, OptionSet<WebEvent::Modifier> modifiers, TransactionID layerTreeTransactionIdAtLastTouchStart)
{
    process().send(Messages::WebPage::HandleDoubleTapForDoubleClickAtPoint(point, modifiers, layerTreeTransactionIdAtLastTouchStart), m_webPageID);
}

void WebPageProxy::inspectorNodeSearchMovedToPosition(const WebCore::FloatPoint& position)
{
    process().send(Messages::WebPage::InspectorNodeSearchMovedToPosition(position), m_webPageID);
}

void WebPageProxy::inspectorNodeSearchEndedAtPosition(const WebCore::FloatPoint& position)
{
    process().send(Messages::WebPage::InspectorNodeSearchEndedAtPosition(position), m_webPageID);
}

void WebPageProxy::blurFocusedElement()
{
    process().send(Messages::WebPage::BlurFocusedElement(), m_webPageID);
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

void WebPageProxy::restorePageState(Optional<WebCore::FloatPoint> scrollPosition, const WebCore::FloatPoint& scrollOrigin, const WebCore::FloatBoxExtent& obscuredInsetsOnSave, double scale)
{
    pageClient().restorePageState(scrollPosition, scrollOrigin, obscuredInsetsOnSave, scale);
}

void WebPageProxy::restorePageCenterAndScale(Optional<WebCore::FloatPoint> center, double scale)
{
    pageClient().restorePageCenterAndScale(center, scale);
}

void WebPageProxy::didGetTapHighlightGeometries(uint64_t requestID, const WebCore::Color& color, const Vector<WebCore::FloatQuad>& highlightedQuads, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius, bool nodeHasBuiltInClickHandling)
{
    pageClient().didGetTapHighlightGeometries(requestID, color, highlightedQuads, topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius, nodeHasBuiltInClickHandling);
}

void WebPageProxy::setIsShowingInputViewForFocusedElement(bool showingInputView)
{
    process().send(Messages::WebPage::SetIsShowingInputViewForFocusedElement(showingInputView), m_webPageID);
}

void WebPageProxy::updateInputContextAfterBlurringAndRefocusingElement()
{
    pageClient().updateInputContextAfterBlurringAndRefocusingElement();
}

void WebPageProxy::elementDidFocus(const FocusedElementInformation& information, bool userIsInteracting, bool blurPreviousNode, OptionSet<WebCore::ActivityState::Flag> activityStateChanges, const UserData& userData)
{
    m_pendingInputModeChange = WTF::nullopt;
    m_waitingForPostLayoutEditorStateUpdateAfterFocusingElement = true;

    API::Object* userDataObject = process().transformHandlesToObjects(userData.object()).get();
    if (m_editorState.isMissingPostLayoutData) {
        // FIXME: We should try to eliminate m_deferredElementDidFocusArguments altogether, in favor of only deferring actions that are dependent on post-layout editor state information.
        m_deferredElementDidFocusArguments = makeUnique<ElementDidFocusArguments>(ElementDidFocusArguments { information, userIsInteracting, blurPreviousNode, activityStateChanges, userDataObject });
        return;
    }

    pageClient().elementDidFocus(information, userIsInteracting, blurPreviousNode, activityStateChanges, userDataObject);
}

void WebPageProxy::elementDidBlur()
{
    m_pendingInputModeChange = WTF::nullopt;
    m_waitingForPostLayoutEditorStateUpdateAfterFocusingElement = false;
    m_deferredElementDidFocusArguments = nullptr;
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
    m_pendingInputModeChange = WTF::nullopt;
}

void WebPageProxy::autofillLoginCredentials(const String& username, const String& password)
{
    m_process->send(Messages::WebPage::AutofillLoginCredentials(username, password), m_webPageID);
}

void WebPageProxy::showInspectorHighlight(const WebCore::Highlight& highlight)
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

void WebPageProxy::focusNextFocusedElement(bool isForward, WTF::Function<void(CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction(CallbackBase::Error::Unknown);
        return;
    }
    
    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::focusNextFocusedElement"_s));
    process().send(Messages::WebPage::FocusNextFocusedElement(isForward, callbackID), m_webPageID);
}

void WebPageProxy::setFocusedElementValue(const String& value)
{
    process().send(Messages::WebPage::SetFocusedElementValue(value), m_webPageID);
}

void WebPageProxy::setFocusedElementValueAsNumber(double value)
{
    process().send(Messages::WebPage::SetFocusedElementValueAsNumber(value), m_webPageID);
}

void WebPageProxy::setFocusedElementSelectedIndex(uint32_t index, bool allowMultipleSelection)
{
    process().send(Messages::WebPage::SetFocusedElementSelectedIndex(index, allowMultipleSelection), m_webPageID);
}

void WebPageProxy::didPerformDictionaryLookup(const DictionaryPopupInfo& dictionaryPopupInfo)
{
    pageClient().didPerformDictionaryLookup(dictionaryPopupInfo);
}

void WebPageProxy::savePDFToTemporaryFolderAndOpenWithNativeApplication(const String&, const String&, const IPC::DataReference&, const String&)
{
    notImplemented();
}

void WebPageProxy::openPDFFromTemporaryFolderWithNativeApplication(const String&)
{
    notImplemented();
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

void WebPageProxy::disableDoubleTapGesturesDuringTapIfNecessary(uint64_t requestID)
{
    pageClient().disableDoubleTapGesturesDuringTapIfNecessary(requestID);
}

void WebPageProxy::handleSmartMagnificationInformationForPotentialTap(uint64_t requestID, const WebCore::FloatRect& renderRect, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale, bool nodeIsRootLevel)
{
    pageClient().handleSmartMagnificationInformationForPotentialTap(requestID, renderRect, fitEntireRect, viewportMinimumScale, viewportMaximumScale, nodeIsRootLevel);
}

uint32_t WebPageProxy::computePagesForPrintingAndDrawToPDF(FrameIdentifier frameID, const PrintInfo& printInfo, DrawToPDFCallback::CallbackFunction&& callback)
{
    if (!hasRunningProcess()) {
        callback(IPC::DataReference(), CallbackBase::Error::OwnerWasInvalidated);
        return 0;
    }

    uint32_t pageCount = 0;
    auto callbackID = m_callbacks.put(WTFMove(callback), m_process->throttler().backgroundActivity("WebPageProxy::computePagesForPrintingAndDrawToPDF"_s));
    using Message = Messages::WebPage::ComputePagesForPrintingAndDrawToPDF;
    process().sendSync(Message(frameID, printInfo, callbackID), Message::Reply(pageCount), m_webPageID, Seconds::infinity());
    return pageCount;
}

void WebPageProxy::contentSizeCategoryDidChange(const String& contentSizeCategory)
{
    process().send(Messages::WebPage::ContentSizeCategoryDidChange(contentSizeCategory), m_webPageID);
}

void WebPageProxy::generateSyntheticEditingCommand(WebKit::SyntheticEditingCommandType command)
{
    if (!hasRunningProcess())
        return;

    process().send(Messages::WebPage::GenerateSyntheticEditingCommand(command), m_webPageID);
}

void WebPageProxy::updateEditorState(const EditorState& editorState)
{
    bool couldChangeSecureInputState = m_editorState.isInPasswordField != editorState.isInPasswordField || m_editorState.selectionIsNone;
    
    m_editorState = editorState;
    
    // Selection being none is a temporary state when editing. Flipping secure input state too quickly was causing trouble (not fully understood).
    if (couldChangeSecureInputState && !editorState.selectionIsNone)
        pageClient().updateSecureInputState();
    
    if (editorState.shouldIgnoreSelectionChanges)
        return;
    
    // We always need to notify the client on iOS to make sure the selection is redrawn,
    // even during composition to support phrase boundary gesture.
    pageClient().selectionDidChange();
    updateFontAttributesAfterEditorStateChange();
}

void WebPageProxy::dispatchDidReceiveEditorStateAfterFocus()
{
    if (!m_waitingForPostLayoutEditorStateUpdateAfterFocusingElement || m_editorState.isMissingPostLayoutData)
        return;

    pageClient().didReceiveEditorStateUpdateAfterFocus();
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

    m_process->connection()->sendWithAsyncReply(Messages::WebPage::RequestEvasionRectsAboveSelection(), WTFMove(callback), m_webPageID);
}

void WebPageProxy::updateSelectionWithDelta(int64_t locationDelta, int64_t lengthDelta, CompletionHandler<void()>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler();
        return;
    }

    m_process->connection()->sendWithAsyncReply(Messages::WebPage::UpdateSelectionWithDelta(locationDelta, lengthDelta), WTFMove(completionHandler), m_webPageID);
}

void WebPageProxy::requestDocumentEditingContext(WebKit::DocumentEditingContextRequest request, CompletionHandler<void(WebKit::DocumentEditingContext)>&& completionHandler)
{
    if (!hasRunningProcess()) {
        completionHandler({ });
        return;
    }

    m_process->connection()->sendWithAsyncReply(Messages::WebPage::RequestDocumentEditingContext(request), WTFMove(completionHandler), m_webPageID);
}

#if ENABLE(DATA_INTERACTION)

void WebPageProxy::didHandleDragStartRequest(bool started)
{
    pageClient().didHandleDragStartRequest(started);
}

void WebPageProxy::didHandleAdditionalDragItemsRequest(bool added)
{
    pageClient().didHandleAdditionalDragItemsRequest(added);
}

void WebPageProxy::requestDragStart(const WebCore::IntPoint& clientPosition, const WebCore::IntPoint& globalPosition, WebCore::DragSourceAction allowedActions)
{
    if (hasRunningProcess())
        m_process->send(Messages::WebPage::RequestDragStart(clientPosition, globalPosition, allowedActions), m_webPageID);
}

void WebPageProxy::requestAdditionalItemsForDragSession(const IntPoint& clientPosition, const IntPoint& globalPosition, WebCore::DragSourceAction allowedActions)
{
    if (hasRunningProcess())
        m_process->send(Messages::WebPage::RequestAdditionalItemsForDragSession(clientPosition, globalPosition, allowedActions), m_webPageID);
}

void WebPageProxy::insertDroppedImagePlaceholders(const Vector<IntSize>& imageSizes, CompletionHandler<void(const Vector<IntRect>&, Optional<WebCore::TextIndicatorData>)>&& completionHandler)
{
    if (hasRunningProcess())
        m_process->connection()->sendWithAsyncReply(Messages::WebPage::InsertDroppedImagePlaceholders(imageSizes), WTFMove(completionHandler), m_webPageID);
    else
        completionHandler({ }, WTF::nullopt);
}

void WebPageProxy::willReceiveEditDragSnapshot()
{
    pageClient().willReceiveEditDragSnapshot();
}

void WebPageProxy::didReceiveEditDragSnapshot(Optional<TextIndicatorData> data)
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
#if PLATFORM(MACCATALYST)
        supportsDesktopClassBrowsing = true;
#else
        supportsDesktopClassBrowsing = currentUserInterfaceIdiomIsPad();
#endif
    });
    return supportsDesktopClassBrowsing;
}

#if !PLATFORM(MACCATALYST)

static bool webViewSizeIsNarrow(WebCore::IntSize viewSize)
{
    return !viewSize.isEmpty() && viewSize.width() <= 375;
}

#endif // !PLATFORM(MACCATALYST)

static bool desktopClassBrowsingRecommendedForRequest(const WebCore::ResourceRequest& request)
{
    // FIXME: This should be additionally gated on site-specific quirks being enabled. However, site-specific quirks are already
    // disabled by default in WKWebView, so we would need a new preference for controlling site-specific quirks that are on-by-default
    // in all apps, but may be turned off via SPI (or via Web Inspector). See also: <rdar://problem/50035167>.
    auto host = request.url().host();
    if (equalLettersIgnoringASCIICase(host, "tv.kakao.com") || host.endsWithIgnoringASCIICase(".tv.kakao.com"))
        return false;

    if (equalLettersIgnoringASCIICase(host, "tving.com") || host.endsWithIgnoringASCIICase(".tving.com"))
        return false;

    if (equalLettersIgnoringASCIICase(host, "live.iqiyi.com") || host.endsWithIgnoringASCIICase(".live.iqiyi.com"))
        return false;

    if (equalLettersIgnoringASCIICase(host, "jsfiddle.net") || host.endsWithIgnoringASCIICase(".jsfiddle.net"))
        return false;

    if (equalLettersIgnoringASCIICase(host, "video.sina.com.cn") || host.endsWithIgnoringASCIICase(".video.sina.com.cn"))
        return false;

    if (equalLettersIgnoringASCIICase(host, "huya.com") || host.endsWithIgnoringASCIICase(".huya.com"))
        return false;

    if (equalLettersIgnoringASCIICase(host, "video.tudou.com") || host.endsWithIgnoringASCIICase(".video.tudou.com"))
        return false;

    if (equalLettersIgnoringASCIICase(host, "cctv.com") || host.endsWithIgnoringASCIICase(".cctv.com"))
        return false;

    if (equalLettersIgnoringASCIICase(host, "v.china.com.cn"))
        return false;

    if (equalLettersIgnoringASCIICase(host, "trello.com") || host.endsWithIgnoringASCIICase(".trello.com"))
        return false;

    if (equalLettersIgnoringASCIICase(host, "ted.com") || host.endsWithIgnoringASCIICase(".ted.com"))
        return false;

    if (host.containsIgnoringASCIICase("hsbc.")) {
        if (equalLettersIgnoringASCIICase(host, "hsbc.com.au") || host.endsWithIgnoringASCIICase(".hsbc.com.au"))
            return false;
        if (equalLettersIgnoringASCIICase(host, "hsbc.com.eg") || host.endsWithIgnoringASCIICase(".hsbc.com.eg"))
            return false;
        if (equalLettersIgnoringASCIICase(host, "hsbc.lk") || host.endsWithIgnoringASCIICase(".hsbc.lk"))
            return false;
        if (equalLettersIgnoringASCIICase(host, "hsbc.co.uk") || host.endsWithIgnoringASCIICase(".hsbc.co.uk"))
            return false;
        if (equalLettersIgnoringASCIICase(host, "hsbc.com.hk") || host.endsWithIgnoringASCIICase(".hsbc.com.hk"))
            return false;
        if (equalLettersIgnoringASCIICase(host, "hsbc.com.mx") || host.endsWithIgnoringASCIICase(".hsbc.com.mx"))
            return false;
        if (equalLettersIgnoringASCIICase(host, "hsbc.ca") || host.endsWithIgnoringASCIICase(".hsbc.ca"))
            return false;
        if (equalLettersIgnoringASCIICase(host, "hsbc.com.ar") || host.endsWithIgnoringASCIICase(".hsbc.com.ar"))
            return false;
        if (equalLettersIgnoringASCIICase(host, "hsbc.com.ph") || host.endsWithIgnoringASCIICase(".hsbc.com.ph"))
            return false;
        if (equalLettersIgnoringASCIICase(host, "hsbc.com") || host.endsWithIgnoringASCIICase(".hsbc.com"))
            return false;
        if (equalLettersIgnoringASCIICase(host, "hsbc.com.cn") || host.endsWithIgnoringASCIICase(".hsbc.com.cn"))
            return false;
    }

    // FIXME: Remove this quirk when <rdar://problem/59480381> is complete.
    if (equalLettersIgnoringASCIICase(host, "fidelity.com") || host.endsWithIgnoringASCIICase(".fidelity.com"))
        return false;

    return true;
}

enum class IgnoreAppCompatibilitySafeguards : bool { No, Yes };
static bool desktopClassBrowsingRecommended(const WebCore::ResourceRequest& request, WebCore::IntSize viewSize, IgnoreAppCompatibilitySafeguards ignoreSafeguards)
{
    if (!desktopClassBrowsingRecommendedForRequest(request))
        return false;

#if !PLATFORM(MACCATALYST)
    if (webViewSizeIsNarrow(viewSize))
        return false;
#endif

    static bool shouldRecommendDesktopClassBrowsing = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
#if PLATFORM(MACCATALYST)
        UNUSED_PARAM(ignoreSafeguards);
        shouldRecommendDesktopClassBrowsing = true;
#else
        // While desktop-class browsing is supported on all iPad models, it is not recommended for iPad mini.
        auto screenClass = MGGetSInt32Answer(kMGQMainScreenClass, MGScreenClassPad2);
        shouldRecommendDesktopClassBrowsing = screenClass != MGScreenClassPad3 && screenClass != MGScreenClassPad4 && desktopClassBrowsingSupported();
        if (ignoreSafeguards == IgnoreAppCompatibilitySafeguards::No && !linkedOnOrAfter(WebKit::SDKVersion::FirstWithModernCompabilityModeByDefault)) {
            // Opt out apps that haven't yet built against the iOS 13 SDK to limit any incompatibilities as a result of enabling desktop-class browsing by default in
            // WKWebView on appropriately-sized iPad models.
            shouldRecommendDesktopClassBrowsing = false;
        }
#endif
    });
    return shouldRecommendDesktopClassBrowsing;
}

WebContentMode WebPageProxy::effectiveContentModeAfterAdjustingPolicies(API::WebsitePolicies& policies, const WebCore::ResourceRequest& request)
{
    if (m_preferences->mediaSourceEnabled()) {
        // FIXME: This is a compatibility hack to ensure that turning MSE on via the existing preference still enables MSE.
        policies.setMediaSourcePolicy(WebsiteMediaSourcePolicy::Enable);
    }

    auto viewSize = this->viewSize();
    bool useDesktopBrowsingMode;
    switch (policies.preferredContentMode()) {
    case WebContentMode::Recommended: {
        auto ignoreSafeguards = m_navigationClient->shouldBypassContentModeSafeguards() ? IgnoreAppCompatibilitySafeguards::Yes : IgnoreAppCompatibilitySafeguards::No;
        useDesktopBrowsingMode = desktopClassBrowsingRecommended(request, viewSize, ignoreSafeguards);
        break;
    }
    case WebContentMode::Mobile:
        useDesktopBrowsingMode = false;
        break;
    case WebContentMode::Desktop:
        useDesktopBrowsingMode = !policies.allowSiteSpecificQuirksToOverrideContentMode() || desktopClassBrowsingRecommendedForRequest(request);
        break;
    default:
        ASSERT_NOT_REACHED();
        useDesktopBrowsingMode = false;
        break;
    }

    m_preferFasterClickOverDoubleTap = false;

    if (!useDesktopBrowsingMode) {
        policies.setAllowContentChangeObserverQuirk(true);
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

bool WebPageProxy::shouldUseForegroundPriorityForClientNavigation() const
{
    // The client may request that we do client navigations at foreground priority, even if the
    // view is not visible, as long as the application is foreground.
    if (!configuration().clientNavigationsRunAtForegroundPriority())
        return false;

    if (isViewVisible())
        return false;

    return pageClient().isApplicationVisible();
}

} // namespace WebKit

#undef RELEASE_LOG_IF_ALLOWED
#undef MESSAGE_CHECK

#endif // PLATFORM(IOS_FAMILY)
