/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeDrawingAreaProxyMac.h"

#if PLATFORM(MAC)

#import "DrawingArea.h"
#import "DrawingAreaMessages.h"
#import "MessageSenderInlines.h"
#import "RemoteLayerTreeScrollingPerformanceData.h"
#import "RemoteScrollingCoordinatorProxyMac.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/DeprecatedGlobalSettings.h>
#import <WebCore/FloatPoint.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/NSScrollerImpDetails.h>
#import <WebCore/ScrollView.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebKit {
using namespace WebCore;

static NSString * const transientAnimationKey = @"wkTransientZoomScale";

class RemoteLayerTreeDisplayLinkClient final : public DisplayLink::Client {
public:
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RemoteLayerTreeDisplayLinkClient(WebPageProxyIdentifier pageID)
        : m_pageIdentifier(pageID)
    {
    }

private:
    void displayLinkFired(WebCore::PlatformDisplayID, WebCore::DisplayUpdate, bool wantsFullSpeedUpdates, bool anyObserverWantsCallback) override;

    WebPageProxyIdentifier m_pageIdentifier;
};

// This is called off the main thread.
void RemoteLayerTreeDisplayLinkClient::displayLinkFired(WebCore::PlatformDisplayID /* displayID */, WebCore::DisplayUpdate /* displayUpdate */, bool /* wantsFullSpeedUpdates */, bool /* anyObserverWantsCallback */)
{
    RunLoop::main().dispatch([pageIdentifier = m_pageIdentifier]() {
        auto page = WebProcessProxy::webPage(pageIdentifier);
        if (!page)
            return;

        if (auto* drawingArea = dynamicDowncast<RemoteLayerTreeDrawingAreaProxy>(page->drawingArea()))
            drawingArea->didRefreshDisplay();
    });
}

RemoteLayerTreeDrawingAreaProxyMac::RemoteLayerTreeDrawingAreaProxyMac(WebPageProxy& pageProxy, WebProcessProxy& webProcessProxy)
    : RemoteLayerTreeDrawingAreaProxy(pageProxy, webProcessProxy)
    , m_displayLinkClient(makeUnique<RemoteLayerTreeDisplayLinkClient>(pageProxy.identifier()))
{
}

RemoteLayerTreeDrawingAreaProxyMac::~RemoteLayerTreeDrawingAreaProxyMac()
{
    if (auto* displayLink = existingDisplayLink()) {
        if (m_fullSpeedUpdateObserverID)
            displayLink->removeObserver(*m_displayLinkClient, *m_fullSpeedUpdateObserverID);
        if (m_displayRefreshObserverID)
            displayLink->removeObserver(*m_displayLinkClient, *m_displayRefreshObserverID);
    }
}

DelegatedScrollingMode RemoteLayerTreeDrawingAreaProxyMac::delegatedScrollingMode() const
{
    return DelegatedScrollingMode::DelegatedToWebKit;
}

std::unique_ptr<RemoteScrollingCoordinatorProxy> RemoteLayerTreeDrawingAreaProxyMac::createScrollingCoordinatorProxy() const
{
    return makeUnique<RemoteScrollingCoordinatorProxyMac>(protectedWebPageProxy());
}

DisplayLink* RemoteLayerTreeDrawingAreaProxyMac::existingDisplayLink()
{
    if (!m_displayID)
        return nullptr;
    
    return m_webPageProxy->process().processPool().displayLinks().existingDisplayLinkForDisplay(*m_displayID);
}

DisplayLink& RemoteLayerTreeDrawingAreaProxyMac::displayLink()
{
    ASSERT(m_displayID);

    auto& displayLinks = m_webPageProxy->process().processPool().displayLinks();
    return displayLinks.displayLinkForDisplay(*m_displayID);
}

void RemoteLayerTreeDrawingAreaProxyMac::removeObserver(std::optional<DisplayLinkObserverID>& observerID)
{
    if (!observerID)
        return;

    if (auto* displayLink = existingDisplayLink())
        displayLink->removeObserver(*m_displayLinkClient, *observerID);

    observerID = { };
}

void RemoteLayerTreeDrawingAreaProxyMac::layoutBannerLayers(const RemoteLayerTreeTransaction& transaction)
{
    auto* headerBannerLayer = m_webPageProxy->headerBannerLayer();
    auto* footerBannerLayer = m_webPageProxy->footerBannerLayer();
    if (!headerBannerLayer && !footerBannerLayer)
        return;

    float totalContentsHeight = transaction.contentsSize().height();

    auto layoutBannerLayer = [](CALayer *bannerLayer, float y, float width) {
        if (bannerLayer.frame.origin.y == y && bannerLayer.frame.size.width == width)
            return;
        [CATransaction begin];
        [CATransaction setAnimationDuration:0];
        [CATransaction setDisableActions:YES];
        [bannerLayer setFrame:CGRectMake(0, y, width, bannerLayer.frame.size.height)];
        [CATransaction commit];
    };

    float topContentInset = m_webPageProxy->scrollingCoordinatorProxy()->topContentInset();
    auto scrollPosition = m_webPageProxy->scrollingCoordinatorProxy()->currentMainFrameScrollPosition();
    
    if (headerBannerLayer) {
        auto headerHeight = headerBannerLayer.frame.size.height;
        totalContentsHeight += headerHeight;
        auto y = LocalFrameView::yPositionForHeaderLayer(scrollPosition, topContentInset);
        layoutBannerLayer(headerBannerLayer, y, size().width());
    }

    if (footerBannerLayer) {
        auto footerHeight = footerBannerLayer.frame.size.height;
        totalContentsHeight += footerBannerLayer.frame.size.height;
        auto y = LocalFrameView::yPositionForFooterLayer(scrollPosition, topContentInset, totalContentsHeight, footerHeight);
        layoutBannerLayer(footerBannerLayer, y, size().width());
    }
}

void RemoteLayerTreeDrawingAreaProxyMac::didCommitLayerTree(IPC::Connection&, const RemoteLayerTreeTransaction& transaction, const RemoteScrollingCoordinatorTransaction&)
{
    m_pageScalingLayerID = transaction.pageScalingLayerID();

    if (m_transientZoomScale)
        applyTransientZoomToLayer();
    else if (m_transactionIDAfterEndingTransientZoom && transaction.transactionID() >= m_transactionIDAfterEndingTransientZoom) {
        removeTransientZoomFromLayer();
        m_transactionIDAfterEndingTransientZoom = { };
    }
    if (m_usesOverlayScrollbars != m_webPageProxy->scrollingCoordinatorProxy()->overlayScrollbarsEnabled()) {
        m_usesOverlayScrollbars = m_webPageProxy->scrollingCoordinatorProxy()->overlayScrollbarsEnabled();
        WebCore::DeprecatedGlobalSettings::setUsesOverlayScrollbars(m_usesOverlayScrollbars);
        
        ScrollerStyle::setUseOverlayScrollbars(m_usesOverlayScrollbars);
        
        NSScrollerStyle style = m_usesOverlayScrollbars ? NSScrollerStyleOverlay : NSScrollerStyleLegacy;
        [NSScrollerImpPair _updateAllScrollerImpPairsForNewRecommendedScrollerStyle:style];
    }

    protectedWebPageProxy()->setScrollPerformanceDataCollectionEnabled(m_webPageProxy->scrollingCoordinatorProxy()->scrollingPerformanceTestingEnabled());
    
    if (transaction.createdLayers().size() > 0) {
        if (WebKit::RemoteLayerTreeScrollingPerformanceData* scrollPerfData = m_webPageProxy->scrollingPerformanceData())
            scrollPerfData->didCommitLayerTree(LayoutRect(transaction.scrollPosition(),  transaction.baseLayoutViewportSize()));
    }

    layoutBannerLayers(transaction);
}

static RetainPtr<CABasicAnimation> transientZoomTransformOverrideAnimation(const TransformationMatrix& transform)
{
    RetainPtr<CABasicAnimation> animation = [CABasicAnimation animationWithKeyPath:@"transform"];
    [animation setDuration:std::numeric_limits<double>::max()];
    [animation setFillMode:kCAFillModeForwards];
    [animation setAdditive:NO];
    [animation setRemovedOnCompletion:false];
    [animation setTimingFunction:[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear]];

    NSValue *transformValue = [NSValue valueWithCATransform3D:transform];
    [animation setFromValue:transformValue];
    [animation setToValue:transformValue];

    return animation;
}

void RemoteLayerTreeDrawingAreaProxyMac::applyTransientZoomToLayer()
{
    ASSERT(m_transientZoomScale);
    ASSERT(m_transientZoomOrigin);

    CALayer *layerForPageScale = remoteLayerTreeHost().layerForID(m_pageScalingLayerID);
    if (!layerForPageScale)
        return;

    TransformationMatrix transform;
    transform.translate(m_transientZoomOrigin->x(), m_transientZoomOrigin->y());
    transform.scale(*m_transientZoomScale);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    auto animationWithScale = transientZoomTransformOverrideAnimation(transform);
    [layerForPageScale removeAnimationForKey:transientAnimationKey];
    [layerForPageScale addAnimation:animationWithScale.get() forKey:transientAnimationKey];
    END_BLOCK_OBJC_EXCEPTIONS
}

void RemoteLayerTreeDrawingAreaProxyMac::removeTransientZoomFromLayer()
{
    CALayer *layerForPageScale = remoteLayerTreeHost().layerForID(m_pageScalingLayerID);
    if (!layerForPageScale)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [layerForPageScale removeAnimationForKey:transientAnimationKey];
    END_BLOCK_OBJC_EXCEPTIONS
}

void RemoteLayerTreeDrawingAreaProxyMac::adjustTransientZoom(double scale, FloatPoint origin)
{
    LOG_WITH_STREAM(ViewGestures, stream << "RemoteLayerTreeDrawingAreaProxyMac::adjustTransientZoom - scale " << scale << " origin " << origin);

    m_transientZoomScale = scale;
    m_transientZoomOrigin = origin;

    applyTransientZoomToLayer();
    
    // FIXME: Only send these messages as fast as the web process is responding to them.
    send(Messages::DrawingArea::AdjustTransientZoom(scale, origin));
}

void RemoteLayerTreeDrawingAreaProxyMac::commitTransientZoom(double scale, FloatPoint origin)
{
    auto visibleContentRect = m_webPageProxy->scrollingCoordinatorProxy()->computeVisibleContentRect();
    
    auto constrainedOrigin = visibleContentRect.location();
    constrainedOrigin.moveBy(-origin);

    IntSize scaledTotalContentsSize = roundedIntSize(m_webPageProxy->scrollingCoordinatorProxy()->totalContentsSize());
    scaledTotalContentsSize.scale(scale / m_webPageProxy->scrollingCoordinatorProxy()->mainFrameScaleFactor());

    LOG_WITH_STREAM(Scrolling, stream << "RemoteLayerTreeDrawingAreaProxyMac::commitTransientZoom constrainScrollPositionForOverhang - constrainedOrigin: " << constrainedOrigin << " visibleContentRect: " << visibleContentRect << " scaledTotalContentsSize: " << scaledTotalContentsSize << "scrollOrigin :" << m_webPageProxy->scrollingCoordinatorProxy()->scrollOrigin() << "headerHeight :" << m_webPageProxy->scrollingCoordinatorProxy()->headerHeight() << " footerHeight : " << m_webPageProxy->scrollingCoordinatorProxy()->footerHeight());
    
    // Scaling may have exposed the overhang area, so we need to constrain the final
    // layer position exactly like scrolling will once it's committed, to ensure that
    // scrolling doesn't make the view jump.
    constrainedOrigin = ScrollableArea::constrainScrollPositionForOverhang(roundedIntRect(visibleContentRect), scaledTotalContentsSize, roundedIntPoint(constrainedOrigin), m_webPageProxy->scrollingCoordinatorProxy()->scrollOrigin(), m_webPageProxy->scrollingCoordinatorProxy()->headerHeight(), m_webPageProxy->scrollingCoordinatorProxy()->footerHeight());
    constrainedOrigin.moveBy(-visibleContentRect.location());
    constrainedOrigin = -constrainedOrigin;
    
    LOG_WITH_STREAM(Scrolling, stream << "RemoteLayerTreeDrawingAreaProxyMac::commitTransientZoom - origin " << origin << " constrained to " << constrainedOrigin << ", scale " << scale);

    auto transientZoomScale = std::exchange(m_transientZoomScale, { });
    auto transientZoomOrigin = std::exchange(m_transientZoomOrigin, { });

    auto rootScrollingNodeID = m_webPageProxy->scrollingCoordinatorProxy()->rootScrollingNodeID();
    m_webPageProxy->scrollingCoordinatorProxy()->deferWheelEventTestCompletionForReason(rootScrollingNodeID, WheelEventTestMonitorDeferReason::CommittingTransientZoom);

    if (transientZoomScale == scale && roundedIntPoint(*transientZoomOrigin) == roundedIntPoint(constrainedOrigin)) {
        // We're already at the right scale and position, so we don't need to animate.
        sendCommitTransientZoom(scale, origin, rootScrollingNodeID);
        return;
    }

    TransformationMatrix transform;
    transform.translate(constrainedOrigin.x(), constrainedOrigin.y());
    transform.scale(scale);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [CATransaction begin];
    CALayer *layerForPageScale = remoteLayerTreeHost().layerForID(m_pageScalingLayerID);
    RetainPtr<CABasicAnimation> renderViewAnimationCA = DrawingArea::transientZoomSnapAnimationForKeyPath("transform"_s);
    NSValue *transformValue = [NSValue valueWithCATransform3D:transform];
    [renderViewAnimationCA setToValue:transformValue];


    [CATransaction setCompletionBlock:[scale, constrainedOrigin, transform, rootScrollingNodeID, protectedWebPageProxy = this->protectedWebPageProxy()] () {
        if (auto* drawingArea = static_cast<RemoteLayerTreeDrawingAreaProxyMac*>(protectedWebPageProxy->drawingArea())) {
            CALayer *layerForPageScale = drawingArea->remoteLayerTreeHost().layerForID(drawingArea->pageScalingLayerID());
            if (layerForPageScale) {
                layerForPageScale.transform = transform;
                [layerForPageScale removeAnimationForKey:transientAnimationKey];
                [layerForPageScale removeAnimationForKey:@"transientZoomCommit"];
            }

            drawingArea->sendCommitTransientZoom(scale, constrainedOrigin, rootScrollingNodeID);
        }
    }];

    [layerForPageScale addAnimation:renderViewAnimationCA.get() forKey:@"transientZoomCommit"];
    [CATransaction commit];
    END_BLOCK_OBJC_EXCEPTIONS
}

void RemoteLayerTreeDrawingAreaProxyMac::sendCommitTransientZoom(double scale, FloatPoint origin, WebCore::ScrollingNodeID rootNodeID)
{
    updateZoomTransactionID();
    sendWithAsyncReply(Messages::DrawingArea::CommitTransientZoom(scale, origin), [rootNodeID, protectedWebPageProxy = this->protectedWebPageProxy()] {
        if (auto* scrollingCoordinatorProxy = protectedWebPageProxy->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->removeWheelEventTestCompletionDeferralForReason(rootNodeID, WheelEventTestMonitorDeferReason::CommittingTransientZoom);
    });
}

void RemoteLayerTreeDrawingAreaProxyMac::scheduleDisplayRefreshCallbacks()
{
    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] RemoteLayerTreeDrawingAreaProxyMac " << this << " scheduleDisplayLink for display " << m_displayID << " - existing observer " << m_displayRefreshObserverID);
    if (m_displayRefreshObserverID)
        return;

    if (!m_displayID) {
        WTFLogAlways("RemoteLayerTreeDrawingAreaProxyMac::scheduleDisplayLink(): page has no displayID");
        return;
    }

    auto& displayLink = this->displayLink();
    m_displayRefreshObserverID = DisplayLinkObserverID::generate();
    displayLink.addObserver(*m_displayLinkClient, *m_displayRefreshObserverID, m_clientPreferredFramesPerSecond);
}

void RemoteLayerTreeDrawingAreaProxyMac::pauseDisplayRefreshCallbacks()
{
    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] RemoteLayerTreeDrawingAreaProxyMac " << this << " pauseDisplayLink for display " << m_displayID << " - observer " << m_displayRefreshObserverID);
    removeObserver(m_displayRefreshObserverID);
}

void RemoteLayerTreeDrawingAreaProxyMac::setPreferredFramesPerSecond(WebCore::FramesPerSecond preferredFramesPerSecond)
{
    m_clientPreferredFramesPerSecond = preferredFramesPerSecond;

    if (!m_displayID) {
        WTFLogAlways("RemoteLayerTreeDrawingAreaProxyMac::scheduleDisplayLink(): page has no displayID");
        return;
    }

    auto* displayLink = existingDisplayLink();
    if (m_displayRefreshObserverID && displayLink)
        displayLink->setObserverPreferredFramesPerSecond(*m_displayLinkClient, *m_displayRefreshObserverID, preferredFramesPerSecond);
}

void RemoteLayerTreeDrawingAreaProxyMac::setDisplayLinkWantsFullSpeedUpdates(bool wantsFullSpeedUpdates)
{
    if (!m_displayID)
        return;

    auto& displayLink = this->displayLink();

    // Use a second observer for full-speed updates (used to drive scroll animations).
    if (wantsFullSpeedUpdates) {
        if (m_fullSpeedUpdateObserverID)
            return;

        m_fullSpeedUpdateObserverID = DisplayLinkObserverID::generate();
        displayLink.addObserver(*m_displayLinkClient, *m_fullSpeedUpdateObserverID, displayLink.nominalFramesPerSecond());
    } else if (m_fullSpeedUpdateObserverID)
        removeObserver(m_fullSpeedUpdateObserverID);
}

void RemoteLayerTreeDrawingAreaProxyMac::windowScreenDidChange(PlatformDisplayID displayID)
{
    if (displayID == m_displayID)
        return;

    bool hadFullSpeedOberver = m_fullSpeedUpdateObserverID.has_value();
    if (hadFullSpeedOberver)
        removeObserver(m_fullSpeedUpdateObserverID);

    pauseDisplayRefreshCallbacks();

    if (m_displayID)
        m_webPageProxy->scrollingCoordinatorProxy()->windowScreenWillChange();

    m_displayID = displayID;
    m_displayNominalFramesPerSecond = displayNominalFramesPerSecond();

    m_webPageProxy->scrollingCoordinatorProxy()->windowScreenDidChange(displayID, m_displayNominalFramesPerSecond);

    scheduleDisplayRefreshCallbacks();
    if (hadFullSpeedOberver) {
        m_fullSpeedUpdateObserverID = DisplayLinkObserverID::generate();
        if (auto* displayLink = existingDisplayLink())
            displayLink->addObserver(*m_displayLinkClient, *m_fullSpeedUpdateObserverID, displayLink->nominalFramesPerSecond());
    }
}

std::optional<WebCore::FramesPerSecond> RemoteLayerTreeDrawingAreaProxyMac::displayNominalFramesPerSecond()
{
    return displayLink().nominalFramesPerSecond();
}

void RemoteLayerTreeDrawingAreaProxyMac::didRefreshDisplay()
{
    // FIXME: Need to pass WebCore::DisplayUpdate here and filter out non-relevant displays.
    m_webPageProxy->scrollingCoordinatorProxy()->displayDidRefresh(m_displayID.value_or(0));
    RemoteLayerTreeDrawingAreaProxy::didRefreshDisplay();
}

void RemoteLayerTreeDrawingAreaProxyMac::didChangeViewExposedRect()
{
    RemoteLayerTreeDrawingAreaProxy::didChangeViewExposedRect();
    updateDebugIndicatorPosition();
}

void RemoteLayerTreeDrawingAreaProxyMac::colorSpaceDidChange()
{
    send(Messages::DrawingArea::SetColorSpace(m_webPageProxy->colorSpace()));
}

MachSendRight RemoteLayerTreeDrawingAreaProxyMac::createFence()
{
    RetainPtr<CAContext> rootLayerContext = [m_webPageProxy->acceleratedCompositingRootLayer() context];
    if (!rootLayerContext)
        return MachSendRight();

    // Don't fence if we don't have a connection, because the message
    // will likely get dropped on the floor (if the Web process is terminated)
    // or queued up until process launch completes, and there's nothing useful
    // to synchronize in these cases.
    if (!m_webProcessProxy->hasConnection())
        return MachSendRight();

    RefPtr connection = m_webProcessProxy->connection();

    // Don't fence if we have incoming synchronous messages, because we may not
    // be able to reply to the message until the fence times out.
    if (connection->hasIncomingSyncMessage())
        return MachSendRight();

    MachSendRight fencePort = MachSendRight::adopt([rootLayerContext createFencePort]);

    // Invalidate the fence if a synchronous message arrives while it's installed,
    // because we won't be able to reply during the fence-wait.
    uint64_t callbackID = connection->installIncomingSyncMessageCallback([rootLayerContext] {
        [rootLayerContext invalidateFences];
    });
    [CATransaction addCommitHandler:[callbackID, connection = WTFMove(connection)] () mutable {
        connection->uninstallIncomingSyncMessageCallback(callbackID);
    } forPhase:kCATransactionPhasePostCommit];

    return fencePort;
}

void RemoteLayerTreeDrawingAreaProxyMac::updateZoomTransactionID()
{
    m_transactionIDAfterEndingTransientZoom = nextLayerTreeTransactionID();
}


} // namespace WebKit

#endif // PLATFORM(MAC)
