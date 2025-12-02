/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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

#import "APIPageConfiguration.h"
#import "DrawingArea.h"
#import "DrawingAreaMessages.h"
#import "MessageSenderInlines.h"
#import "RemoteLayerTreeCommitBundle.h"
#import "RemoteLayerTreeScrollingPerformanceData.h"
#import "RemoteScrollingCoordinatorProxyMac.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/DeprecatedGlobalSettings.h>
#import <WebCore/FloatPoint.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/NSScrollerImpDetails.h>
#import <WebCore/ScrollView.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeDrawingAreaProxyMac);

static NSString * const transientZoomAnimationKey = @"wkTransientZoomScale";
static NSString * const transientZoomCommitAnimationKey = @"wkTransientZoomCommit";
static NSString * const transientClipPositionAnimationKey = @"wkTransientClipPosition";
static NSString * const transientClipSizeAnimationKey = @"wkTransientClipSize";
static NSString * const transientScrolledContentsPositionAnimationKey = @"wkTransientScrolledContentsPosition";
static NSString * const transientZoomScrollPositionOverrideAnimationKey = @"wkScrollPositionOverride";

class RemoteLayerTreeDisplayLinkClient final : public DisplayLink::Client {
public:
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerTreeDisplayLinkClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteLayerTreeDisplayLinkClient);
public:
    explicit RemoteLayerTreeDisplayLinkClient(WebPageProxyIdentifier pageID)
        : m_pageIdentifier(pageID)
    {
    }

private:
    void displayLinkFired(WebCore::PlatformDisplayID, WebCore::DisplayUpdate, bool wantsFullSpeedUpdates, bool anyObserverWantsCallback) override;

    WebPageProxyIdentifier m_pageIdentifier;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeDisplayLinkClient);

// This is called off the main thread.
void RemoteLayerTreeDisplayLinkClient::displayLinkFired(WebCore::PlatformDisplayID /* displayID */, WebCore::DisplayUpdate /* displayUpdate */, bool /* wantsFullSpeedUpdates */, bool /* anyObserverWantsCallback */)
{
    RunLoop::mainSingleton().dispatch([pageIdentifier = m_pageIdentifier]() {
        auto page = WebProcessProxy::webPage(pageIdentifier);
        if (!page)
            return;

        if (RefPtr drawingArea = dynamicDowncast<RemoteLayerTreeDrawingAreaProxy>(page->drawingArea()))
            drawingArea->didRefreshDisplay();
    });
}

Ref<RemoteLayerTreeDrawingAreaProxyMac> RemoteLayerTreeDrawingAreaProxyMac::create(WebPageProxy& page, WebProcessProxy& webProcessProxy)
{
    return adoptRef(*new RemoteLayerTreeDrawingAreaProxyMac(page, webProcessProxy));
}

RemoteLayerTreeDrawingAreaProxyMac::RemoteLayerTreeDrawingAreaProxyMac(WebPageProxy& pageProxy, WebProcessProxy& webProcessProxy)
    : RemoteLayerTreeDrawingAreaProxy(pageProxy, webProcessProxy)
    , m_displayLinkClient(makeUniqueRef<RemoteLayerTreeDisplayLinkClient>(pageProxy.identifier()))
{
}

RemoteLayerTreeDrawingAreaProxyMac::~RemoteLayerTreeDrawingAreaProxyMac()
{
    if (auto* displayLink = existingDisplayLink()) {
        if (m_fullSpeedUpdateObserverID)
            displayLink->removeObserver(m_displayLinkClient, *m_fullSpeedUpdateObserverID);
        if (m_displayRefreshObserverID)
            displayLink->removeObserver(m_displayLinkClient, *m_displayRefreshObserverID);
    }
}

DelegatedScrollingMode RemoteLayerTreeDrawingAreaProxyMac::delegatedScrollingMode() const
{
    return DelegatedScrollingMode::DelegatedToWebKit;
}

std::unique_ptr<RemoteScrollingCoordinatorProxy> RemoteLayerTreeDrawingAreaProxyMac::createScrollingCoordinatorProxy() const
{
    return makeUnique<RemoteScrollingCoordinatorProxyMac>(*protectedPage());
}

DisplayLink* RemoteLayerTreeDrawingAreaProxyMac::existingDisplayLink()
{
    if (!m_displayID)
        return nullptr;

    RefPtr page = this->page();
    if (!page)
        return nullptr;

    return page->configuration().processPool().displayLinks().existingDisplayLinkForDisplay(*m_displayID);
}

DisplayLink& RemoteLayerTreeDrawingAreaProxyMac::displayLink()
{
    ASSERT(m_displayID);

    auto& displayLinks = protectedPage()->configuration().processPool().displayLinks();
    return displayLinks.displayLinkForDisplay(*m_displayID);
}

void RemoteLayerTreeDrawingAreaProxyMac::removeObserver(std::optional<DisplayLinkObserverID>& observerID)
{
    if (!observerID)
        return;

    if (auto* displayLink = existingDisplayLink())
        displayLink->removeObserver(m_displayLinkClient, *observerID);

    observerID = { };
}

void RemoteLayerTreeDrawingAreaProxyMac::layoutBannerLayers(const RemoteLayerTreeTransaction& transaction)
{
    RefPtr webPageProxy = page();
    if (!webPageProxy)
        return;

    RetainPtr headerBannerLayer = webPageProxy->headerBannerLayer();
    RetainPtr footerBannerLayer = webPageProxy->footerBannerLayer();
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

    CheckedRef scrollingCoordinatorProxy = *webPageProxy->scrollingCoordinatorProxy();
    float topInset = scrollingCoordinatorProxy->obscuredContentInsets().top();
    auto scrollPosition = scrollingCoordinatorProxy->currentMainFrameScrollPosition();
    
    if (headerBannerLayer) {
        auto headerHeight = headerBannerLayer.get().frame.size.height;
        totalContentsHeight += headerHeight;
        auto y = LocalFrameView::yPositionForHeaderLayer(scrollPosition, topInset);
        layoutBannerLayer(headerBannerLayer.get(), y, size().width());
    }

    if (footerBannerLayer) {
        auto footerHeight = footerBannerLayer.get().frame.size.height;
        totalContentsHeight += footerBannerLayer.get().frame.size.height;
        auto y = LocalFrameView::yPositionForFooterLayer(scrollPosition, topInset, totalContentsHeight, footerHeight);
        layoutBannerLayer(footerBannerLayer.get(), y, size().width());
    }
}

void RemoteLayerTreeDrawingAreaProxyMac::didCommitLayerTree(IPC::Connection&, const RemoteLayerTreeTransaction& transaction, const RemoteScrollingCoordinatorTransaction&, const std::optional<MainFrameData>& mainFrameData, const TransactionID& transactionID)
{
    if (!mainFrameData)
        return;

    RefPtr page = this->page();
    const auto& mainFrameCommitData = *mainFrameData;

    m_pageScalingLayerID = mainFrameCommitData.pageScalingLayerID;
    m_pageScrollingLayerID = mainFrameCommitData.scrolledContentsLayerID;
    m_scrolledContentsLayerID = mainFrameCommitData.scrolledContentsLayerID;
    m_mainFrameClipLayerID = mainFrameCommitData.mainFrameClipLayerID;

    if (m_transientZoomScale)
        applyTransientZoomToLayer();
    else if (m_transactionIDAfterEndingTransientZoom && transactionID.greaterThanOrEqualSameProcess(*m_transactionIDAfterEndingTransientZoom)) {
        removeTransientZoomFromLayer();
        m_transactionIDAfterEndingTransientZoom = { };
    }
    CheckedRef scrollingCoordinatorProxy = *page->scrollingCoordinatorProxy();
    auto usesOverlayScrollbars = scrollingCoordinatorProxy->overlayScrollbarsEnabled();
    auto newScrollbarStyle = usesOverlayScrollbars ? ScrollbarStyle::Overlay : ScrollbarStyle::AlwaysVisible;
    if (!m_scrollbarStyle || m_scrollbarStyle != newScrollbarStyle) {
        m_scrollbarStyle = newScrollbarStyle;
        WebCore::DeprecatedGlobalSettings::setUsesOverlayScrollbars(usesOverlayScrollbars);
        
        ScrollerStyle::setUseOverlayScrollbars(usesOverlayScrollbars);
        
        NSScrollerStyle style = usesOverlayScrollbars ? NSScrollerStyleOverlay : NSScrollerStyleLegacy;
        [NSScrollerImpPair _updateAllScrollerImpPairsForNewRecommendedScrollerStyle:style];
    }

    page->setScrollPerformanceDataCollectionEnabled(scrollingCoordinatorProxy->scrollingPerformanceTestingEnabled());

    if (transaction.createdLayers().size() > 0) {
        if (WebKit::RemoteLayerTreeScrollingPerformanceData* scrollPerfData = page->scrollingPerformanceData())
            scrollPerfData->didCommitLayerTree(LayoutRect(transaction.scrollPosition(), mainFrameCommitData.baseLayoutViewportSize));
    }

    layoutBannerLayers(transaction);
}

static RetainPtr<CABasicAnimation> fillFowardsAnimationWithKeyPath(NSString *keyPath)
{
    RetainPtr<CABasicAnimation> animation = [CABasicAnimation animationWithKeyPath:keyPath];
    [animation setDuration:std::numeric_limits<double>::max()];
    [animation setFillMode:kCAFillModeForwards];
    [animation setAdditive:NO];
    [animation setRemovedOnCompletion:false];
    [animation setTimingFunction:[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear]];

    return animation;
}

static RetainPtr<CABasicAnimation> fillFowardsAnimationWithKeyPathAndValue(NSString *keyPath, NSValue *value)
{
    RetainPtr animation = fillFowardsAnimationWithKeyPath(keyPath);
    [animation setFromValue:value];
    [animation setToValue:value];
    return animation;
}

static RetainPtr<CABasicAnimation> transientZoomTransformOverrideAnimation(const TransformationMatrix& transform)
{
    return fillFowardsAnimationWithKeyPathAndValue(@"transform", [NSValue valueWithCATransform3D:transform]);
}

static RetainPtr<CABasicAnimation> transientSizeAnimation(const FloatSize& size)
{
    return fillFowardsAnimationWithKeyPathAndValue(@"bounds.size", [NSValue valueWithSize:size]);
}

static RetainPtr<CABasicAnimation> transientPositionAnimation(const FloatPoint& position)
{
    return fillFowardsAnimationWithKeyPathAndValue(@"position", [NSValue valueWithPoint:position]);
}

void RemoteLayerTreeDrawingAreaProxyMac::applyTransientZoomToLayer()
{
    ASSERT(m_transientZoomScale);
    ASSERT(m_transientZoomOriginInLayerForPageScale);
    ASSERT(m_transientZoomOriginInVisibleRect);

    RetainPtr layerForPageScale = remoteLayerTreeHost().layerForID(m_pageScalingLayerID);
    if (!layerForPageScale)
        return;

    TransformationMatrix transform;
    transform.translate(m_transientZoomOriginInLayerForPageScale->x(), m_transientZoomOriginInLayerForPageScale->y());
    transform.scale(*m_transientZoomScale);

    RetainPtr clipLayer = remoteLayerTreeHost().layerForID(m_mainFrameClipLayerID);
    RetainPtr scrolledContentsLayer = remoteLayerTreeHost().layerForID(m_scrolledContentsLayerID);

    auto scaleForClipLayerAdjustment = std::max(1.0, *m_transientZoomScale);
    auto clipLayerPosition = FloatPoint { [clipLayer position] };
    auto clipLayerZoomOrigin = clipLayerPosition + *m_transientZoomOriginInVisibleRect;
    auto transientClipLayerFrame = scaledRectAtOrigin([clipLayer frame], scaleForClipLayerAdjustment, clipLayerZoomOrigin);
    auto transientScrolledContentsPosition = FloatPoint { [scrolledContentsLayer position] } + (clipLayerPosition - transientClipLayerFrame.location());

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    auto animationWithScale = transientZoomTransformOverrideAnimation(transform);
    [layerForPageScale removeAnimationForKey:transientZoomAnimationKey];
    [layerForPageScale addAnimation:animationWithScale.get() forKey:transientZoomAnimationKey];
    [clipLayer removeAnimationForKey:transientClipPositionAnimationKey];
    [clipLayer removeAnimationForKey:transientClipSizeAnimationKey];
    [clipLayer addAnimation:transientPositionAnimation(transientClipLayerFrame.location()).get() forKey:transientClipPositionAnimationKey];
    [clipLayer addAnimation:transientSizeAnimation(transientClipLayerFrame.size()).get() forKey:transientClipSizeAnimationKey];
    [scrolledContentsLayer removeAnimationForKey:transientScrolledContentsPositionAnimationKey];
    [scrolledContentsLayer addAnimation:transientPositionAnimation(transientScrolledContentsPosition).get() forKey:transientScrolledContentsPositionAnimationKey];
    END_BLOCK_OBJC_EXCEPTIONS
}

void RemoteLayerTreeDrawingAreaProxyMac::removeTransientZoomFromLayer()
{
    RetainPtr layerForPageScale = remoteLayerTreeHost().layerForID(m_pageScalingLayerID);
    if (!layerForPageScale)
        return;

    RetainPtr clipLayer = remoteLayerTreeHost().layerForID(m_mainFrameClipLayerID);
    RetainPtr scrolledContentsLayer = remoteLayerTreeHost().layerForID(m_scrolledContentsLayerID);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [layerForPageScale removeAnimationForKey:transientZoomAnimationKey];
    [clipLayer removeAnimationForKey:transientClipPositionAnimationKey];
    [clipLayer removeAnimationForKey:transientClipSizeAnimationKey];
    [scrolledContentsLayer removeAnimationForKey:transientScrolledContentsPositionAnimationKey];
    END_BLOCK_OBJC_EXCEPTIONS
}

void RemoteLayerTreeDrawingAreaProxyMac::adjustTransientZoom(double scale, FloatPoint originInLayerForPageScale, WebCore::FloatPoint originInVisibleRect)
{
    LOG_WITH_STREAM(ViewGestures, stream << "RemoteLayerTreeDrawingAreaProxyMac::adjustTransientZoom - scale " << scale << " originInLayerForPageScale " << originInLayerForPageScale << " originInVisibleRect " << originInVisibleRect);

    m_transientZoomScale = scale;
    m_transientZoomOriginInLayerForPageScale = originInLayerForPageScale;
    m_transientZoomOriginInVisibleRect = originInVisibleRect;

    applyTransientZoomToLayer();
    
    // FIXME: Only send these messages as fast as the web process is responding to them.
    send(Messages::DrawingArea::AdjustTransientZoom(scale, originInLayerForPageScale));
}

void RemoteLayerTreeDrawingAreaProxyMac::commitTransientZoom(double scale, FloatPoint origin)
{
    RefPtr page = this->page();
    if (!page)
        return;

    CheckedRef scrollingCoordinatorProxy = *page->scrollingCoordinatorProxy();
    auto visibleContentRect = scrollingCoordinatorProxy->computeVisibleContentRect();
    
    auto constrainedOrigin = visibleContentRect.location();
    constrainedOrigin.moveBy(-origin);

    IntSize scaledTotalContentsSize = roundedIntSize(scrollingCoordinatorProxy->totalContentsSize());
    scaledTotalContentsSize.scale(scale / scrollingCoordinatorProxy->mainFrameScaleFactor());

    LOG_WITH_STREAM(ViewGestures, stream << "RemoteLayerTreeDrawingAreaProxyMac::commitTransientZoom constrainScrollPositionForOverhang - constrainedOrigin: " << constrainedOrigin << " visibleContentRect: " << visibleContentRect << " scaledTotalContentsSize: " << scaledTotalContentsSize << " scrollOrigin:" << scrollingCoordinatorProxy->scrollOrigin() << " headerHeight:" << scrollingCoordinatorProxy->headerHeight() << " footerHeight: " << scrollingCoordinatorProxy->footerHeight());

    // Scaling may have exposed the overhang area, so we need to constrain the final
    // layer position exactly like scrolling will once it's committed, to ensure that
    // scrolling doesn't make the view jump.
    constrainedOrigin = ScrollableArea::constrainScrollPositionForOverhang(roundedIntRect(visibleContentRect), scaledTotalContentsSize, roundedIntPoint(constrainedOrigin), scrollingCoordinatorProxy->scrollOrigin(), scrollingCoordinatorProxy->headerHeight(), scrollingCoordinatorProxy->footerHeight());
    constrainedOrigin.moveBy(-visibleContentRect.location());
    constrainedOrigin = -constrainedOrigin;
    
    LOG_WITH_STREAM(ViewGestures, stream << "RemoteLayerTreeDrawingAreaProxyMac::commitTransientZoom - origin " << origin << " constrained to " << constrainedOrigin << ", scale " << scale);

    auto transientZoomScale = std::exchange(m_transientZoomScale, { });
    auto transientZoomOrigin = std::exchange(m_transientZoomOriginInLayerForPageScale, { });
    m_transientZoomOriginInVisibleRect = { };

    auto rootScrollingNodeID = scrollingCoordinatorProxy->rootScrollingNodeID();
    if (rootScrollingNodeID)
        scrollingCoordinatorProxy->deferWheelEventTestCompletionForReason(rootScrollingNodeID, WheelEventTestMonitorDeferReason::CommittingTransientZoom);

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

    RetainPtr layerForPageScale = remoteLayerTreeHost().layerForID(m_pageScalingLayerID);
    auto renderViewAnimationCA = DrawingArea::transientZoomSnapAnimationForKeyPath("transform"_s);
    RetainPtr transformValue = [NSValue valueWithCATransform3D:transform];
    [renderViewAnimationCA setToValue:transformValue.get()];

    RetainPtr layerForPageScrolling = remoteLayerTreeHost().layerForID(m_pageScrollingLayerID);
    auto scrollPositionOverrideAnimation = fillFowardsAnimationWithKeyPath(@"position");
    RetainPtr pointValue = [NSValue valueWithPoint:NSPointFromCGPoint(layerForPageScrolling.get().position)];
    [scrollPositionOverrideAnimation setFromValue:pointValue.get()];
    [scrollPositionOverrideAnimation setToValue:pointValue.get()];
    [layerForPageScrolling addAnimation:scrollPositionOverrideAnimation.get() forKey:transientZoomScrollPositionOverrideAnimationKey];

    [CATransaction setCompletionBlock:[scale, constrainedOrigin, rootScrollingNodeID, page] () {
        if (RefPtr drawingArea = downcast<RemoteLayerTreeDrawingAreaProxyMac>(page->drawingArea()))
            drawingArea->sendCommitTransientZoom(scale, constrainedOrigin, rootScrollingNodeID);

        page->callAfterNextPresentationUpdate([page] {
            RefPtr drawingArea = downcast<RemoteLayerTreeDrawingAreaProxyMac>(page->drawingArea());
            if (!drawingArea)
                return;

            BEGIN_BLOCK_OBJC_EXCEPTIONS
            if (RetainPtr layerForPageScale = drawingArea->remoteLayerTreeHost().layerForID(drawingArea->pageScalingLayerID())) {
                [layerForPageScale removeAnimationForKey:transientZoomAnimationKey];
                [layerForPageScale removeAnimationForKey:transientZoomCommitAnimationKey];
            }

            if (RetainPtr layerForPageScrolling = drawingArea->remoteLayerTreeHost().layerForID(drawingArea->m_pageScrollingLayerID))
                [layerForPageScrolling removeAnimationForKey:transientZoomScrollPositionOverrideAnimationKey];

            END_BLOCK_OBJC_EXCEPTIONS
        });
    }];

    RetainPtr clipLayer = remoteLayerTreeHost().layerForID(m_mainFrameClipLayerID);
    RetainPtr scrolledContentsLayer = remoteLayerTreeHost().layerForID(m_scrolledContentsLayerID);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [clipLayer removeAnimationForKey:transientClipPositionAnimationKey];
    [clipLayer removeAnimationForKey:transientClipSizeAnimationKey];
    [scrolledContentsLayer removeAnimationForKey:transientScrolledContentsPositionAnimationKey];
    END_BLOCK_OBJC_EXCEPTIONS

    [layerForPageScale addAnimation:renderViewAnimationCA.get() forKey:transientZoomCommitAnimationKey];

    [CATransaction commit];

    END_BLOCK_OBJC_EXCEPTIONS
}

void RemoteLayerTreeDrawingAreaProxyMac::sendCommitTransientZoom(double scale, FloatPoint origin, std::optional<WebCore::ScrollingNodeID> rootNodeID)
{
    updateZoomTransactionID();

    RefPtr webPageProxy = page();
    if (!webPageProxy)
        return;

    webPageProxy->scalePageRelativeToScrollPosition(scale, roundedIntPoint(origin));

    if (!rootNodeID)
        return;

    webPageProxy->callAfterNextPresentationUpdate([rootNodeID, webPageProxy]() {
        if (CheckedPtr scrollingCoordinatorProxy = webPageProxy->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->removeWheelEventTestCompletionDeferralForReason(rootNodeID, WheelEventTestMonitorDeferReason::CommittingTransientZoom);
    });

}

void RemoteLayerTreeDrawingAreaProxyMac::scheduleDisplayRefreshCallbacks()
{
    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] RemoteLayerTreeDrawingAreaProxyMac " << this << " scheduleDisplayLink for display " << m_displayID << " - existing observer " << m_displayRefreshObserverID);
    if (m_displayRefreshObserverID)
        return;

    if (!m_displayID) {
        RELEASE_LOG(DisplayLink, "RemoteLayerTreeDrawingAreaProxyMac::scheduleDisplayLink(): page has no displayID");
        return;
    }

    auto& displayLink = this->displayLink();
    m_displayRefreshObserverID = DisplayLinkObserverID::generate();
    displayLink.addObserver(m_displayLinkClient, *m_displayRefreshObserverID, m_clientPreferredFramesPerSecond);
    if (m_shouldLogNextObserverChange) {
        RefPtr webPageProxy = page();
        if (webPageProxy) {
            RELEASE_LOG(ViewState, "%p [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i, DisplayID=%u] RemoteLayerTreeDrawingAreaProxyMac::scheduleDisplayRefreshCallbacks",
                this, webPageProxy->identifier().toUInt64(), webPageProxy->webPageIDInMainFrameProcess().toUInt64(), webPageProxy->legacyMainFrameProcessID(), m_displayID ? *m_displayID : 0);
        }
        m_shouldLogNextObserverChange = false;
    }
}

void RemoteLayerTreeDrawingAreaProxyMac::pauseDisplayRefreshCallbacks()
{
    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] RemoteLayerTreeDrawingAreaProxyMac " << this << " pauseDisplayLink for display " << m_displayID << " - observer " << m_displayRefreshObserverID);
    removeObserver(m_displayRefreshObserverID);
}

void RemoteLayerTreeDrawingAreaProxyMac::setPreferredFramesPerSecond(IPC::Connection& connection,  WebCore::FramesPerSecond preferredFramesPerSecond)
{
    // FIXME(site-isolation): This currently ignores throttling requests from remote subframes (as would also happen for in-process subframes). We have the opportunity to do better, and allow throttling on a per-process level.
    if (!webProcessProxy().hasConnection() || &webProcessProxy().connection() != &connection)
        return;

    m_clientPreferredFramesPerSecond = preferredFramesPerSecond;

    if (!m_displayID) {
        RELEASE_LOG(DisplayLink, "RemoteLayerTreeDrawingAreaProxyMac::scheduleDisplayLink(): page has no displayID");
        return;
    }

    auto* displayLink = existingDisplayLink();
    if (m_displayRefreshObserverID && displayLink)
        displayLink->setObserverPreferredFramesPerSecond(m_displayLinkClient, *m_displayRefreshObserverID, preferredFramesPerSecond);
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
        displayLink.addObserver(m_displayLinkClient, *m_fullSpeedUpdateObserverID, displayLink.nominalFramesPerSecond());
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

    RefPtr page = this->page();
    if (m_displayID && page)
        page->checkedScrollingCoordinatorProxy()->windowScreenWillChange();

    m_displayID = displayID;
    m_displayNominalFramesPerSecond = displayNominalFramesPerSecond();

    if (page)
        page->checkedScrollingCoordinatorProxy()->windowScreenDidChange(displayID, m_displayNominalFramesPerSecond);

    scheduleDisplayRefreshCallbacks();
    if (hadFullSpeedOberver) {
        m_fullSpeedUpdateObserverID = DisplayLinkObserverID::generate();
        if (auto* displayLink = existingDisplayLink())
            displayLink->addObserver(m_displayLinkClient, *m_fullSpeedUpdateObserverID, displayLink->nominalFramesPerSecond());
    }
}

void RemoteLayerTreeDrawingAreaProxyMac::viewIsBecomingVisible()
{
    m_shouldLogNextObserverChange = true;
    m_shouldLogNextDisplayRefresh = true;
}

void RemoteLayerTreeDrawingAreaProxyMac::viewIsBecomingInvisible()
{
    m_shouldLogNextObserverChange = false;
    m_shouldLogNextDisplayRefresh = false;
}

std::optional<WebCore::FramesPerSecond> RemoteLayerTreeDrawingAreaProxyMac::displayNominalFramesPerSecond()
{
    if (!m_displayID)
        return std::nullopt;
    return displayLink().nominalFramesPerSecond();
}

void RemoteLayerTreeDrawingAreaProxyMac::didRefreshDisplay()
{
    RefPtr page = this->page();
    if (m_shouldLogNextDisplayRefresh) {
        if (page) {
            RELEASE_LOG(ViewState, "%p [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i, DisplayID=%u] RemoteLayerTreeDrawingAreaProxyMac::didRefreshDisplay",
                this, page->identifier().toUInt64(), page->webPageIDInMainFrameProcess().toUInt64(), page->legacyMainFrameProcessID(), m_displayID ? *m_displayID : 0);
        }
        m_shouldLogNextDisplayRefresh = false;
    }
    // FIXME: Need to pass WebCore::DisplayUpdate here and filter out non-relevant displays.
    if (page)
        page->checkedScrollingCoordinatorProxy()->displayDidRefresh(m_displayID.value_or(0));
    RemoteLayerTreeDrawingAreaProxy::didRefreshDisplay();
}

void RemoteLayerTreeDrawingAreaProxyMac::didChangeViewExposedRect()
{
    RemoteLayerTreeDrawingAreaProxy::didChangeViewExposedRect();
    updateDebugIndicatorPosition();
}

void RemoteLayerTreeDrawingAreaProxyMac::dispatchSetObscuredContentInsets()
{
    if (RefPtr page = this->page())
        page->dispatchSetObscuredContentInsets();
}

void RemoteLayerTreeDrawingAreaProxyMac::colorSpaceDidChange()
{
    forEachProcessState([&](ProcessState& state, WebProcessProxy& webProcess) {
        if (RefPtr page = this->page())
            webProcess.send(Messages::DrawingArea::SetColorSpace(page->colorSpace()), identifier());
    });
}

MachSendRight RemoteLayerTreeDrawingAreaProxyMac::createFence()
{
    RefPtr page = this->page();
    if (!page)
        return MachSendRight();

    RetainPtr<CAContext> rootLayerContext = [page->protectedAcceleratedCompositingRootLayer() context];
    if (!rootLayerContext)
        return MachSendRight();

    // Don't fence if we don't have a connection, because the message
    // will likely get dropped on the floor (if the Web process is terminated)
    // or queued up until process launch completes, and there's nothing useful
    // to synchronize in these cases.
    if (!webProcessProxy().hasConnection())
        return MachSendRight();

    Ref connection = webProcessProxy().connection();

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
    m_transactionIDAfterEndingTransientZoom = nextMainFrameLayerTreeTransactionID();
}


} // namespace WebKit

#endif // PLATFORM(MAC)
