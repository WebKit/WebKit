/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "APIUIClient.h"
#import "DataReference.h"
#import "EditingRange.h"
#import "InteractionInformationAtPosition.h"
#import "NativeWebKeyboardEvent.h"
#import "PageClient.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteLayerTreeDrawingAreaProxyMessages.h"
#import "RemoteLayerTreeTransaction.h"
#import "UIKitSPI.h"
#import "UserData.h"
#import "ViewUpdateDispatcherMessages.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WebPageMessages.h"
#import "WebProcessProxy.h"
#import "WebVideoFullscreenManagerProxy.h"
#import <WebCore/FrameView.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/UserAgent.h>

#if USE(QUICK_LOOK)
#import "APILoaderClient.h"
#import "APINavigationClient.h"
#import <wtf/text/WTFString.h>
#endif

using namespace WebCore;

namespace WebKit {

void WebPageProxy::platformInitialize()
{
}

static String webKitBundleVersionString()
{
    return [[NSBundle bundleForClass:NSClassFromString(@"WKWebView")] objectForInfoDictionaryKey:(NSString *)kCFBundleVersionKey];
}

String WebPageProxy::standardUserAgent(const String& applicationNameForUserAgent)
{
    return standardUserAgentWithApplicationName(applicationNameForUserAgent, webKitBundleVersionString());
}

void WebPageProxy::getIsSpeaking(bool&)
{
    notImplemented();
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

void WebPageProxy::performDictionaryLookupAtLocation(const WebCore::FloatPoint&)
{
    notImplemented();
}

void WebPageProxy::gestureCallback(const WebCore::IntPoint& point, uint32_t gestureType, uint32_t gestureState, uint32_t flags, uint64_t callbackID)
{
    auto callback = m_callbacks.take<GestureCallback>(callbackID);
    if (!callback) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    callback->performCallbackWithReturnValue(point, gestureType, gestureState, flags);
}

void WebPageProxy::touchesCallback(const WebCore::IntPoint& point, uint32_t touches, uint64_t callbackID)
{
    auto callback = m_callbacks.take<TouchesCallback>(callbackID);
    if (!callback) {
        ASSERT_NOT_REACHED();
        return;
    }

    callback->performCallbackWithReturnValue(point, touches);
}

void WebPageProxy::autocorrectionDataCallback(const Vector<WebCore::FloatRect>& rects, const String& fontName, float fontSize, uint64_t fontTraits, uint64_t callbackID)
{
    auto callback = m_callbacks.take<AutocorrectionDataCallback>(callbackID);
    if (!callback) {
        ASSERT_NOT_REACHED();
        return;
    }

    callback->performCallbackWithReturnValue(rects, fontName, fontSize, fontTraits);
}

void WebPageProxy::dictationContextCallback(const String& selectedText, const String& beforeText, const String& afterText, uint64_t callbackID)
{
    auto callback = m_callbacks.take<DictationContextCallback>(callbackID);
    if (!callback) {
        ASSERT_NOT_REACHED();
        return;
    }

    callback->performCallbackWithReturnValue(selectedText, beforeText, afterText);
}

void WebPageProxy::autocorrectionContextCallback(const String& beforeText, const String& markedText, const String& selectedText, const String& afterText, uint64_t location, uint64_t length, uint64_t callbackID)
{
    auto callback = m_callbacks.take<AutocorrectionContextCallback>(callbackID);
    if (!callback) {
        ASSERT_NOT_REACHED();
        return;
    }

    callback->performCallbackWithReturnValue(beforeText, markedText, selectedText, afterText, location, length);
}

void WebPageProxy::updateVisibleContentRects(const WebCore::FloatRect& exposedRect, const WebCore::FloatRect& unobscuredRect, const WebCore::FloatRect& unobscuredRectInScrollViewCoordinates, const WebCore::FloatRect& customFixedPositionRect, double scale, bool inStableState, bool isChangingObscuredInsetsInteractively, bool allowShrinkToFit, double timestamp, double horizontalVelocity, double verticalVelocity, double scaleChangeRate)
{
    if (!isValid())
        return;

    VisibleContentRectUpdateInfo visibleContentRectUpdateInfo(exposedRect, unobscuredRect, unobscuredRectInScrollViewCoordinates, customFixedPositionRect, scale, inStableState, isChangingObscuredInsetsInteractively, allowShrinkToFit, timestamp, horizontalVelocity, verticalVelocity, scaleChangeRate, downcast<RemoteLayerTreeDrawingAreaProxy>(*drawingArea()).lastCommittedLayerTreeTransactionID());

    if (visibleContentRectUpdateInfo == m_lastVisibleContentRectUpdate)
        return;

    m_lastVisibleContentRectUpdate = visibleContentRectUpdateInfo;
    m_process->send(Messages::ViewUpdateDispatcher::VisibleContentRectUpdate(m_pageID, visibleContentRectUpdateInfo), 0);
}

void WebPageProxy::resendLastVisibleContentRects()
{
    m_process->send(Messages::ViewUpdateDispatcher::VisibleContentRectUpdate(m_pageID, m_lastVisibleContentRectUpdate), 0);
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

WebCore::FloatRect WebPageProxy::computeCustomFixedPositionRect(const FloatRect& unobscuredContentRect, double displayedContentScale, UnobscuredRectConstraint constraint) const
{
    FloatRect constrainedUnobscuredRect = unobscuredContentRect;
    FloatRect documentRect = m_pageClient.documentRect();

    if (m_pageClient.isAssistingNode())
        return documentRect;

    if (constraint == UnobscuredRectConstraint::ConstrainedToDocumentRect)
        constrainedUnobscuredRect.intersect(documentRect);

    double minimumScale = m_pageClient.minimumZoomScale();
    
    if (displayedContentScale < minimumScale) {
        const CGFloat slope = 12;
        CGFloat factor = std::max<CGFloat>(1 - slope * (minimumScale - displayedContentScale), 0);
            
        constrainedUnobscuredRect.setX(adjustedUnexposedEdge(documentRect.x(), constrainedUnobscuredRect.x(), factor));
        constrainedUnobscuredRect.setY(adjustedUnexposedEdge(documentRect.y(), constrainedUnobscuredRect.y(), factor));
        constrainedUnobscuredRect.setWidth(adjustedUnexposedMaxEdge(documentRect.maxX(), constrainedUnobscuredRect.maxX(), factor) - constrainedUnobscuredRect.x());
        constrainedUnobscuredRect.setHeight(adjustedUnexposedMaxEdge(documentRect.maxY(), constrainedUnobscuredRect.maxY(), factor) - constrainedUnobscuredRect.y());
    }
    
    return FrameView::rectForViewportConstrainedObjects(enclosingLayoutRect(constrainedUnobscuredRect), LayoutSize(documentRect.size()), displayedContentScale, false, StickToViewportBounds);
}

void WebPageProxy::overflowScrollViewWillStartPanGesture()
{
    m_pageClient.overflowScrollViewWillStartPanGesture();
}

void WebPageProxy::overflowScrollViewDidScroll()
{
    m_pageClient.overflowScrollViewDidScroll();
}

void WebPageProxy::overflowScrollWillStartScroll()
{
    m_pageClient.overflowScrollWillStartScroll();
}

void WebPageProxy::overflowScrollDidEndScroll()
{
    m_pageClient.overflowScrollDidEndScroll();
}

void WebPageProxy::dynamicViewportSizeUpdate(const FloatSize& minimumLayoutSize, const WebCore::FloatSize& maximumUnobscuredSize, const FloatRect& targetExposedContentRect, const FloatRect& targetUnobscuredRect, const FloatRect& targetUnobscuredRectInScrollViewCoordinates,  double targetScale, int32_t deviceOrientation)
{
    if (!isValid())
        return;

    m_dynamicViewportSizeUpdateWaitingForTarget = true;
    m_dynamicViewportSizeUpdateWaitingForLayerTreeCommit = true;
    m_process->send(Messages::WebPage::DynamicViewportSizeUpdate(minimumLayoutSize, maximumUnobscuredSize, targetExposedContentRect, targetUnobscuredRect, targetUnobscuredRectInScrollViewCoordinates, targetScale, deviceOrientation, ++m_currentDynamicViewportSizeUpdateID), m_pageID);
}

void WebPageProxy::synchronizeDynamicViewportUpdate()
{
    if (!isValid())
        return;

    if (m_dynamicViewportSizeUpdateWaitingForTarget) {
        // We do not want the UIProcess to finish animated resize with the old content size, scale, etc.
        // If that happens, the UIProcess would start pushing new VisibleContentRectUpdateInfo to the WebProcess with
        // invalid informations.
        //
        // Ideally, the animated resize should just be transactional, and the UIProcess would remain in the "resize" state
        // until both DynamicViewportUpdateChangedTarget and the associated commitLayerTree are finished.
        // The tricky part with such implementation is if a second animated resize starts before the end of the previous one.
        // In that case, the values used for the target state needs to be computed from the output of the previous animated resize.
        //
        // The following is a workaround to have the UIProcess in a consistent state.
        // Instead of handling nested resize, we block the UIProcess until the animated resize finishes.
        double newScale;
        FloatPoint newScrollPosition;
        uint64_t nextValidLayerTreeTransactionID;
        if (m_process->sendSync(Messages::WebPage::SynchronizeDynamicViewportUpdate(), Messages::WebPage::SynchronizeDynamicViewportUpdate::Reply(newScale, newScrollPosition, nextValidLayerTreeTransactionID), m_pageID, std::chrono::seconds(2))) {
            m_dynamicViewportSizeUpdateWaitingForTarget = false;
            m_dynamicViewportSizeUpdateLayerTreeTransactionID = nextValidLayerTreeTransactionID;
            m_pageClient.dynamicViewportUpdateChangedTarget(newScale, newScrollPosition, nextValidLayerTreeTransactionID);
        }

    }

    // If m_dynamicViewportSizeUpdateWaitingForTarget is false, we are waiting for the next valid frame with the hope it is the one for the new target.
    // If m_dynamicViewportSizeUpdateWaitingForTarget is still true, this is a desesperate attempt to get the valid frame before finishing the animation.
    if (m_dynamicViewportSizeUpdateWaitingForLayerTreeCommit)
        m_process->connection()->waitForAndDispatchImmediately<Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree>(m_pageID, std::chrono::seconds(1), IPC::InterruptWaitingIfSyncMessageArrives);

    m_dynamicViewportSizeUpdateWaitingForTarget = false;
    m_dynamicViewportSizeUpdateWaitingForLayerTreeCommit = false;
}

void WebPageProxy::setViewportConfigurationMinimumLayoutSize(const WebCore::FloatSize& size)
{
    m_process->send(Messages::WebPage::SetViewportConfigurationMinimumLayoutSize(size), m_pageID);
}

void WebPageProxy::updateForceAlwaysUserScalable()
{
    m_process->send(Messages::WebPage::UpdateForceAlwaysUserScalable(), m_pageID);
}

void WebPageProxy::setMaximumUnobscuredSize(const WebCore::FloatSize& size)
{
    m_process->send(Messages::WebPage::SetMaximumUnobscuredSize(size), m_pageID);
}

void WebPageProxy::setDeviceOrientation(int32_t deviceOrientation)
{
    if (deviceOrientation != m_deviceOrientation) {
        m_deviceOrientation = deviceOrientation;
        if (isValid())
            m_process->send(Messages::WebPage::SetDeviceOrientation(deviceOrientation), m_pageID);
    }
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

    if (!m_dynamicViewportSizeUpdateWaitingForTarget && m_dynamicViewportSizeUpdateWaitingForLayerTreeCommit) {
        if (layerTreeTransaction.transactionID() >= m_dynamicViewportSizeUpdateLayerTreeTransactionID)
            m_dynamicViewportSizeUpdateWaitingForLayerTreeCommit = false;
    }

    m_pageClient.didCommitLayerTree(layerTreeTransaction);

    // FIXME: Remove this special mechanism and fold it into the transaction's layout milestones.
    if (m_wantsSessionRestorationRenderTreeSizeThresholdEvent && !m_hitRenderTreeSizeThreshold
        && exceedsRenderTreeSizeSizeThreshold(m_sessionRestorationRenderTreeSize, layerTreeTransaction.renderTreeSize())) {
        m_hitRenderTreeSizeThreshold = true;
        didLayout(WebCore::ReachedSessionRestorationRenderTreeSizeThreshold);
    }

    if (m_hasDeferredStartAssistingNode) {
        m_pageClient.startAssistingNode(m_deferredNodeAssistanceArguments->m_nodeInformation, m_deferredNodeAssistanceArguments->m_userIsInteracting, m_deferredNodeAssistanceArguments->m_blurPreviousNode, m_deferredNodeAssistanceArguments->m_userData.get());
        m_hasDeferredStartAssistingNode = false;
        m_deferredNodeAssistanceArguments = nullptr;
    }
}

void WebPageProxy::selectWithGesture(const WebCore::IntPoint point, WebCore::TextGranularity granularity, uint32_t gestureType, uint32_t gestureState, bool isInteractingWithAssistedNode, std::function<void (const WebCore::IntPoint&, uint32_t, uint32_t, uint32_t, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(WebCore::IntPoint(), 0, 0, 0, CallbackBase::Error::Unknown);
        return;
    }

    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::SelectWithGesture(point, (uint32_t)granularity, gestureType, gestureState, isInteractingWithAssistedNode, callbackID), m_pageID);
}

void WebPageProxy::updateSelectionWithTouches(const WebCore::IntPoint point, uint32_t touches, bool baseIsStart, std::function<void (const WebCore::IntPoint&, uint32_t, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(WebCore::IntPoint(), 0, CallbackBase::Error::Unknown);
        return;
    }

    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::UpdateSelectionWithTouches(point, touches, baseIsStart, callbackID), m_pageID);
}
    
void WebPageProxy::replaceDictatedText(const String& oldText, const String& newText)
{
    m_process->send(Messages::WebPage::ReplaceDictatedText(oldText, newText), m_pageID);
}

void WebPageProxy::replaceSelectedText(const String& oldText, const String& newText)
{
    m_process->send(Messages::WebPage::ReplaceSelectedText(oldText, newText), m_pageID);
}

void WebPageProxy::requestAutocorrectionData(const String& textForAutocorrection, std::function<void (const Vector<WebCore::FloatRect>&, const String&, double, uint64_t, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(Vector<WebCore::FloatRect>(), String(), 0, 0, CallbackBase::Error::Unknown);
        return;
    }

    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::RequestAutocorrectionData(textForAutocorrection, callbackID), m_pageID);
}

void WebPageProxy::applyAutocorrection(const String& correction, const String& originalText, std::function<void (const String&, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), CallbackBase::Error::Unknown);
        return;
    }

    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::ApplyAutocorrection(correction, originalText, callbackID), m_pageID);
}

void WebPageProxy::executeEditCommand(const String& commandName, std::function<void (CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::ExecuteEditCommandWithCallback(commandName, callbackID), m_pageID);
}

bool WebPageProxy::applyAutocorrection(const String& correction, const String& originalText)
{
    bool autocorrectionApplied = false;
    m_process->sendSync(Messages::WebPage::SyncApplyAutocorrection(correction, originalText), Messages::WebPage::SyncApplyAutocorrection::Reply(autocorrectionApplied), m_pageID);
    return autocorrectionApplied;
}

void WebPageProxy::selectTextWithGranularityAtPoint(const WebCore::IntPoint point, WebCore::TextGranularity granularity, bool isInteractingWithAssistedNode, std::function<void (CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::SelectTextWithGranularityAtPoint(point, static_cast<uint32_t>(granularity), isInteractingWithAssistedNode, callbackID), m_pageID);
}

void WebPageProxy::selectPositionAtBoundaryWithDirection(const WebCore::IntPoint point, WebCore::TextGranularity granularity, WebCore::SelectionDirection direction, bool isInteractingWithAssistedNode, std::function<void (CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::SelectPositionAtBoundaryWithDirection(point, static_cast<uint32_t>(granularity), static_cast<uint32_t>(direction), isInteractingWithAssistedNode, callbackID), m_pageID);
}

void WebPageProxy::moveSelectionAtBoundaryWithDirection(WebCore::TextGranularity granularity, WebCore::SelectionDirection direction, std::function<void(CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::MoveSelectionAtBoundaryWithDirection(static_cast<uint32_t>(granularity), static_cast<uint32_t>(direction), callbackID), m_pageID);
}
    
void WebPageProxy::selectPositionAtPoint(const WebCore::IntPoint point, bool isInteractingWithAssistedNode, std::function<void (CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::SelectPositionAtPoint(point, isInteractingWithAssistedNode, callbackID), m_pageID);
}

void WebPageProxy::beginSelectionInDirection(WebCore::SelectionDirection direction, std::function<void (uint64_t, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(0, CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::BeginSelectionInDirection(direction, callbackID), m_pageID);
}

void WebPageProxy::updateSelectionWithExtentPoint(const WebCore::IntPoint point, bool isInteractingWithAssistedNode, std::function<void (uint64_t, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(0, CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::UpdateSelectionWithExtentPoint(point, isInteractingWithAssistedNode, callbackID), m_pageID);
    
}

void WebPageProxy::updateSelectionWithExtentPointAndBoundary(const WebCore::IntPoint point, WebCore::TextGranularity granularity, bool isInteractingWithAssistedNode, std::function<void(uint64_t, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(0, CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::UpdateSelectionWithExtentPointAndBoundary(point, granularity, isInteractingWithAssistedNode, callbackID), m_pageID);
    
}

void WebPageProxy::requestDictationContext(std::function<void (const String&, const String&, const String&, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), String(), String(), CallbackBase::Error::Unknown);
        return;
    }

    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::RequestDictationContext(callbackID), m_pageID);
}

void WebPageProxy::requestAutocorrectionContext(std::function<void (const String&, const String&, const String&, const String&, uint64_t, uint64_t, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), String(), String(), String(), 0, 0, CallbackBase::Error::Unknown);
        return;
    }

    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::RequestAutocorrectionContext(callbackID), m_pageID);
}

void WebPageProxy::getAutocorrectionContext(String& beforeContext, String& markedText, String& selectedText, String& afterContext, uint64_t& location, uint64_t& length)
{
    m_process->sendSync(Messages::WebPage::GetAutocorrectionContext(), Messages::WebPage::GetAutocorrectionContext::Reply(beforeContext, markedText, selectedText, afterContext, location, length), m_pageID);
}

void WebPageProxy::getLookupContextAtPoint(const WebCore::IntPoint& point, std::function<void(const String&, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::GetLookupContextAtPoint(point, callbackID), m_pageID);
}

void WebPageProxy::handleTwoFingerTapAtPoint(const WebCore::IntPoint& point, std::function<void(const String&, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(String(), CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    process().send(Messages::WebPage::HandleTwoFingerTapAtPoint(point, callbackID), m_pageID);
}

void WebPageProxy::selectWithTwoTouches(const WebCore::IntPoint from, const WebCore::IntPoint to, uint32_t gestureType, uint32_t gestureState, std::function<void (const WebCore::IntPoint&, uint32_t, uint32_t, uint32_t, CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(WebCore::IntPoint(), 0, 0, 0, CallbackBase::Error::Unknown);
        return;
    }

    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::SelectWithTwoTouches(from, to, gestureType, gestureState, callbackID), m_pageID);
}

void WebPageProxy::updateBlockSelectionWithTouch(const WebCore::IntPoint point, uint32_t touch, uint32_t handlePosition)
{
    m_process->send(Messages::WebPage::UpdateBlockSelectionWithTouch(point, touch, handlePosition), m_pageID);
}

void WebPageProxy::didReceivePositionInformation(const InteractionInformationAtPosition& info)
{
    m_pageClient.positionInformationDidChange(info);
}

void WebPageProxy::getPositionInformation(const WebCore::IntPoint& point, InteractionInformationAtPosition& info)
{
    m_process->sendSync(Messages::WebPage::GetPositionInformation(point), Messages::WebPage::GetPositionInformation::Reply(info), m_pageID);
}

void WebPageProxy::requestPositionInformation(const WebCore::IntPoint& point)
{
    m_process->send(Messages::WebPage::RequestPositionInformation(point), m_pageID);
}

void WebPageProxy::startInteractionWithElementAtPosition(const WebCore::IntPoint& point)
{
    m_process->send(Messages::WebPage::StartInteractionWithElementAtPosition(point), m_pageID);
}

void WebPageProxy::stopInteraction()
{
    m_process->send(Messages::WebPage::StopInteraction(), m_pageID);
}

void WebPageProxy::performActionOnElement(uint32_t action)
{
    m_process->send(Messages::WebPage::PerformActionOnElement(action), m_pageID);
}

void WebPageProxy::saveImageToLibrary(const SharedMemory::Handle& imageHandle, uint64_t imageSize)
{
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::map(imageHandle, SharedMemory::Protection::ReadOnly);
    RefPtr<SharedBuffer> buffer = SharedBuffer::create(static_cast<unsigned char*>(sharedMemoryBuffer->data()), imageSize);
    m_pageClient.saveImageToLibrary(buffer);
}

void WebPageProxy::didUpdateBlockSelectionWithTouch(uint32_t touch, uint32_t flags, float growThreshold, float shrinkThreshold)
{
    m_pageClient.didUpdateBlockSelectionWithTouch(touch, flags, growThreshold, shrinkThreshold);
}

void WebPageProxy::applicationDidEnterBackground()
{
    bool isSuspendedUnderLock = [UIApp isSuspendedUnderLock];
    m_process->send(Messages::WebPage::ApplicationDidEnterBackground(isSuspendedUnderLock), m_pageID);
}

void WebPageProxy::applicationWillEnterForeground()
{
    bool isSuspendedUnderLock = [UIApp isSuspendedUnderLock];
    m_process->send(Messages::WebPage::ApplicationWillEnterForeground(isSuspendedUnderLock), m_pageID);
}

void WebPageProxy::applicationWillResignActive()
{
    m_process->send(Messages::WebPage::ApplicationWillResignActive(), m_pageID);
}

void WebPageProxy::applicationDidBecomeActive()
{
    if (m_videoFullscreenManager)
        m_videoFullscreenManager->applicationDidBecomeActive();
    m_process->send(Messages::WebPage::ApplicationDidBecomeActive(), m_pageID);
}

void WebPageProxy::extendSelection(WebCore::TextGranularity granularity)
{
    m_process->send(Messages::WebPage::ExtendSelection(static_cast<uint32_t>(granularity)), m_pageID);
}

void WebPageProxy::selectWordBackward()
{
    m_process->send(Messages::WebPage::SelectWordBackward(), m_pageID);
}

void WebPageProxy::moveSelectionByOffset(int32_t offset, std::function<void (CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    m_process->send(Messages::WebPage::MoveSelectionByOffset(offset, callbackID), m_pageID);
}

void WebPageProxy::interpretKeyEvent(const EditorState& state, bool isCharEvent, bool& handled)
{
    m_editorState = state;
    handled = m_pageClient.interpretKeyEvent(m_keyEventQueue.first(), isCharEvent);
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
    m_pageClient.accessibilityWebProcessTokenReceived(data);
}    

void WebPageProxy::makeFirstResponder()
{
    notImplemented();
}

void WebPageProxy::registerUIProcessAccessibilityTokens(const IPC::DataReference& elementToken, const IPC::DataReference& windowToken)
{
    if (!isValid())
        return;
    
    process().send(Messages::WebPage::RegisterUIProcessAccessibilityTokens(elementToken, windowToken), m_pageID);
}

void WebPageProxy::pluginFocusOrWindowFocusChanged(uint64_t, bool)
{
    notImplemented();
}

void WebPageProxy::setPluginComplexTextInputState(uint64_t, uint64_t)
{
    notImplemented();
}

void WebPageProxy::executeSavedCommandBySelector(const String&, bool&)
{
    notImplemented();
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
    process().send(Messages::WebPage::WillStartUserTriggeredZooming(), m_pageID);
}

void WebPageProxy::potentialTapAtPosition(const WebCore::FloatPoint& position, uint64_t& requestID)
{
    process().send(Messages::WebPage::PotentialTapAtPosition(requestID, position), m_pageID);
}

void WebPageProxy::commitPotentialTap()
{
    process().send(Messages::WebPage::CommitPotentialTap(m_layerTreeTransactionIdAtLastTouchStart), m_pageID);
}

void WebPageProxy::cancelPotentialTap()
{
    process().send(Messages::WebPage::CancelPotentialTap(), m_pageID);
}

void WebPageProxy::tapHighlightAtPosition(const WebCore::FloatPoint& position, uint64_t& requestID)
{
    process().send(Messages::WebPage::TapHighlightAtPosition(requestID, position), m_pageID);
}

void WebPageProxy::handleTap(const FloatPoint& location)
{
    process().send(Messages::WebPage::HandleTap(roundedIntPoint(location), m_layerTreeTransactionIdAtLastTouchStart), m_pageID);
}

void WebPageProxy::inspectorNodeSearchMovedToPosition(const WebCore::FloatPoint& position)
{
    process().send(Messages::WebPage::InspectorNodeSearchMovedToPosition(position), m_pageID);
}

void WebPageProxy::inspectorNodeSearchEndedAtPosition(const WebCore::FloatPoint& position)
{
    process().send(Messages::WebPage::InspectorNodeSearchEndedAtPosition(position), m_pageID);
}

void WebPageProxy::blurAssistedNode()
{
    process().send(Messages::WebPage::BlurAssistedNode(), m_pageID);
}

FloatSize WebPageProxy::screenSize()
{
    return WebCore::screenSize();
}

FloatSize WebPageProxy::availableScreenSize()
{
    return WebCore::availableScreenSize();
}
    
float WebPageProxy::textAutosizingWidth()
{
    return WebCore::screenSize().width();
}

void WebPageProxy::dynamicViewportUpdateChangedTarget(double newScale, const WebCore::FloatPoint& newScrollPosition, uint64_t dynamicViewportSizeUpdateID)
{
    if (dynamicViewportSizeUpdateID != m_currentDynamicViewportSizeUpdateID)
        return;

    if (m_dynamicViewportSizeUpdateWaitingForTarget) {
        m_dynamicViewportSizeUpdateLayerTreeTransactionID = downcast<RemoteLayerTreeDrawingAreaProxy>(*drawingArea()).nextLayerTreeTransactionID();
        m_dynamicViewportSizeUpdateWaitingForTarget = false;
        m_pageClient.dynamicViewportUpdateChangedTarget(newScale, newScrollPosition, m_dynamicViewportSizeUpdateLayerTreeTransactionID);
    }
}

void WebPageProxy::couldNotRestorePageState()
{
    m_pageClient.couldNotRestorePageState();
}

void WebPageProxy::restorePageState(const WebCore::FloatRect& exposedRect, double scale)
{
    m_pageClient.restorePageState(exposedRect, scale);
}

void WebPageProxy::restorePageCenterAndScale(const WebCore::FloatPoint& center, double scale)
{
    m_pageClient.restorePageCenterAndScale(center, scale);
}

void WebPageProxy::didGetTapHighlightGeometries(uint64_t requestID, const WebCore::Color& color, const Vector<WebCore::FloatQuad>& highlightedQuads, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius)
{
    m_pageClient.didGetTapHighlightGeometries(requestID, color, highlightedQuads, topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius);
}

void WebPageProxy::startAssistingNode(const AssistedNodeInformation& information, bool userIsInteracting, bool blurPreviousNode, const UserData& userData)
{
    API::Object* userDataObject = process().transformHandlesToObjects(userData.object()).get();
    if (m_editorState.isMissingPostLayoutData) {
        m_deferredNodeAssistanceArguments = std::make_unique<NodeAssistanceArguments>(NodeAssistanceArguments { information, userIsInteracting, blurPreviousNode, userDataObject });
        m_hasDeferredStartAssistingNode = true;
        return;
    }

    m_pageClient.startAssistingNode(information, userIsInteracting, blurPreviousNode, userDataObject);
}

void WebPageProxy::stopAssistingNode()
{
    if (m_hasDeferredStartAssistingNode) {
        m_hasDeferredStartAssistingNode = false;
        m_deferredNodeAssistanceArguments = nullptr;
    }
    m_pageClient.stopAssistingNode();
}

void WebPageProxy::showInspectorHighlight(const WebCore::Highlight& highlight)
{
    m_pageClient.showInspectorHighlight(highlight);
}

void WebPageProxy::hideInspectorHighlight()
{
    m_pageClient.hideInspectorHighlight();
}

void WebPageProxy::showInspectorIndication()
{
    m_pageClient.showInspectorIndication();
}

void WebPageProxy::hideInspectorIndication()
{
    m_pageClient.hideInspectorIndication();
}

void WebPageProxy::enableInspectorNodeSearch()
{
    m_pageClient.enableInspectorNodeSearch();
}

void WebPageProxy::disableInspectorNodeSearch()
{
    m_pageClient.disableInspectorNodeSearch();
}

void WebPageProxy::focusNextAssistedNode(bool isForward, std::function<void (CallbackBase::Error)> callbackFunction)
{
    if (!isValid()) {
        callbackFunction(CallbackBase::Error::Unknown);
        return;
    }
    
    uint64_t callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivityToken());
    process().send(Messages::WebPage::FocusNextAssistedNode(isForward, callbackID), m_pageID);
}

void WebPageProxy::setAssistedNodeValue(const String& value)
{
    process().send(Messages::WebPage::SetAssistedNodeValue(value), m_pageID);
}

void WebPageProxy::setAssistedNodeValueAsNumber(double value)
{
    process().send(Messages::WebPage::SetAssistedNodeValueAsNumber(value), m_pageID);
}

void WebPageProxy::setAssistedNodeSelectedIndex(uint32_t index, bool allowMultipleSelection)
{
    process().send(Messages::WebPage::SetAssistedNodeSelectedIndex(index, allowMultipleSelection), m_pageID);
}

void WebPageProxy::didPerformDictionaryLookup(const DictionaryPopupInfo&)
{
    notImplemented();
}

void WebPageProxy::savePDFToTemporaryFolderAndOpenWithNativeApplication(const String&, const String&, const IPC::DataReference&, const String&)
{
    notImplemented();
}

void WebPageProxy::savePDFToTemporaryFolderAndOpenWithNativeApplicationRaw(const String&, const String&, const uint8_t*, unsigned long, const String&)
{
    notImplemented();
}

void WebPageProxy::openPDFFromTemporaryFolderWithNativeApplication(const String&)
{
    notImplemented();
}

void WebPageProxy::setAcceleratedCompositingRootLayer(LayerOrView* rootLayer)
{
    m_pageClient.setAcceleratedCompositingRootLayer(rootLayer);
}

void WebPageProxy::showPlaybackTargetPicker(bool hasVideo, const IntRect& elementRect)
{
    m_pageClient.showPlaybackTargetPicker(hasVideo, elementRect);
}

void WebPageProxy::zoomToRect(FloatRect rect, double minimumScale, double maximumScale)
{
    m_pageClient.zoomToRect(rect, minimumScale, maximumScale);
}

void WebPageProxy::commitPotentialTapFailed()
{
    m_pageClient.commitPotentialTapFailed();
}

void WebPageProxy::didNotHandleTapAsClick(const WebCore::IntPoint& point)
{
    m_pageClient.didNotHandleTapAsClick(point);
    m_uiClient->didNotHandleTapAsClick(point);
}

void WebPageProxy::disableDoubleTapGesturesDuringTapIfNecessary(uint64_t requestID)
{
    m_pageClient.disableDoubleTapGesturesDuringTapIfNecessary(requestID);
}

void WebPageProxy::didFinishDrawingPagesToPDF(const IPC::DataReference& pdfData)
{
    m_pageClient.didFinishDrawingPagesToPDF(pdfData);
}

void WebPageProxy::contentSizeCategoryDidChange(const String& contentSizeCategory)
{
    process().send(Messages::WebPage::ContentSizeCategoryDidChange(contentSizeCategory), m_pageID);
}

void WebPageProxy::editorStateChanged(const EditorState& editorState)
{
    bool couldChangeSecureInputState = m_editorState.isInPasswordField != editorState.isInPasswordField || m_editorState.selectionIsNone;
    
    m_editorState = editorState;
    
    // Selection being none is a temporary state when editing. Flipping secure input state too quickly was causing trouble (not fully understood).
    if (couldChangeSecureInputState && !editorState.selectionIsNone)
        m_pageClient.updateSecureInputState();
    
    if (editorState.shouldIgnoreCompositionSelectionChange)
        return;
    
    // We always need to notify the client on iOS to make sure the selection is redrawn,
    // even during composition to support phrase boundary gesture.
    m_pageClient.selectionDidChange();
}

#if USE(QUICK_LOOK)
    
void WebPageProxy::didStartLoadForQuickLookDocumentInMainFrame(const String& fileName, const String& uti)
{
    // Ensure that fileName isn't really a path name
    static_assert(notFound + 1 == 0, "The following line assumes WTF::notFound equals -1");
    if (m_navigationClient)
        m_navigationClient->didStartLoadForQuickLookDocumentInMainFrame(fileName.substring(fileName.reverseFind('/') + 1), uti);
    else
        m_loaderClient->didStartLoadForQuickLookDocumentInMainFrame(fileName.substring(fileName.reverseFind('/') + 1), uti);
}

void WebPageProxy::didFinishLoadForQuickLookDocumentInMainFrame(const QuickLookDocumentData& data)
{
    if (m_navigationClient)
        m_navigationClient->didFinishLoadForQuickLookDocumentInMainFrame(data);
    else
        m_loaderClient->didFinishLoadForQuickLookDocumentInMainFrame(data);
}
#endif

} // namespace WebKit

#endif // PLATFORM(IOS)
