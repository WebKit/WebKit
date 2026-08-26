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
#import "TransientZoomState.h"
#import "VisibleContentRectUpdateInfo.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/DeprecatedGlobalSettings.h>
#import <WebCore/FloatPoint.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/NSScrollerImpDetails.h>
#import <WebCore/ScrollView.h>
#import <WebCore/VelocityData.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>
#import <wtf/ApproximateTime.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/SetForScope.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringConcatenateNumbers.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeDrawingAreaProxyMac);

static NSString * const transientZoomAnimationKey = @"wkTransientZoomScale";
static NSString * const transientZoomCommitAnimationKey = @"wkTransientZoomCommit";
static NSString * const transientClipPositionAnimationKey = @"wkTransientClipPosition";
static NSString * const transientClipSizeAnimationKey = @"wkTransientClipSize";
static NSString * const transientScrolledContentsPositionAnimationKey = @"wkTransientScrolledContentsPosition";
static NSString * const transientZoomScrollPositionOverrideAnimationKey = @"wkScrollPositionOverride";
static NSString * const delegatedZoomSnapAnimationKey = @"wkDelegatedZoomSnap";

class RemoteLayerTreeDisplayLinkClient final : public DisplayLink::Client, public ThreadSafeRefCounted<RemoteLayerTreeDisplayLinkClient> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerTreeDisplayLinkClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteLayerTreeDisplayLinkClient);
public:
    static Ref<RemoteLayerTreeDisplayLinkClient> create(WebPageProxyIdentifier pageID)
    {
        return adoptRef(*new RemoteLayerTreeDisplayLinkClient(pageID));
    }

private:
    explicit RemoteLayerTreeDisplayLinkClient(WebPageProxyIdentifier pageID)
        : m_pageIdentifier(pageID)
    {
    }

    void displayLinkFired(WebCore::PlatformDisplayID, WebCore::DisplayUpdate, bool wantsFullSpeedUpdates, bool anyObserverWantsCallback) override;

    WebPageProxyIdentifier m_pageIdentifier;
    // NaN means no pending dispatch. Otherwise, stores the ApproximateTime when the pending dispatch was posted.
    std::atomic<double> m_pendingMainThreadDispatchTime { std::numeric_limits<double>::quiet_NaN() };
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeDisplayLinkClient);

// This is called off the main thread.
void RemoteLayerTreeDisplayLinkClient::displayLinkFired(WebCore::PlatformDisplayID /* displayID */, WebCore::DisplayUpdate /* displayUpdate */, bool /* wantsFullSpeedUpdates */, bool /* anyObserverWantsCallback */)
{
    auto now = ApproximateTime::now().secondsSinceEpoch().value();
    auto existingTime = m_pendingMainThreadDispatchTime.load(std::memory_order_relaxed);

    if (!std::isnan(existingTime)) {
        auto pendingDuration = Seconds(now - existingTime);
        static constexpr auto timeoutDuration = 500_ms;
        if (pendingDuration < timeoutDuration)
            return;

        RELEASE_LOG_ERROR(DisplayLink, "RemoteLayerTreeDisplayLinkClient %p: pending main thread dispatch stuck for %.2fs, forcing dispatch", this, pendingDuration.value());
    }

    m_pendingMainThreadDispatchTime.store(now, std::memory_order_relaxed);

    RunLoop::mainSingleton().dispatch([this, protectedThis = Ref { *this }]() {
        m_pendingMainThreadDispatchTime.store(std::numeric_limits<double>::quiet_NaN(), std::memory_order_relaxed);

        RefPtr page = WebProcessProxy::webPage(m_pageIdentifier);
        if (!page)
            return;

        RefPtr drawingArea = dynamicDowncast<RemoteLayerTreeDrawingAreaProxy>(page->drawingArea());
        if (!drawingArea)
            return;

        drawingArea->didRefreshDisplay();
    });
}

Ref<RemoteLayerTreeDrawingAreaProxyMac> RemoteLayerTreeDrawingAreaProxyMac::create(WebPageProxy& page, WebProcessProxy& webProcessProxy)
{
    return adoptRef(*new RemoteLayerTreeDrawingAreaProxyMac(page, webProcessProxy));
}

RemoteLayerTreeDrawingAreaProxyMac::RemoteLayerTreeDrawingAreaProxyMac(WebPageProxy& pageProxy, WebProcessProxy& webProcessProxy)
    : RemoteLayerTreeDrawingAreaProxy(pageProxy, webProcessProxy)
    , m_displayLinkClient(RemoteLayerTreeDisplayLinkClient::create(pageProxy.identifier()))
    , m_processPool(pageProxy.configuration().processPool())
{
}

RemoteLayerTreeDrawingAreaProxyMac::~RemoteLayerTreeDrawingAreaProxyMac()
{
    if (RefPtr processPool = m_processPool.get())
        processPool->displayLinks().stopDisplayLinks(m_displayLinkClient);
}

DelegatedScrollingMode RemoteLayerTreeDrawingAreaProxyMac::delegatedScrollingMode() const
{
    return DelegatedScrollingMode::DelegatedToWebKit;
}

std::unique_ptr<RemoteScrollingCoordinatorProxy> RemoteLayerTreeDrawingAreaProxyMac::createScrollingCoordinatorProxy() const
{
    return makeUnique<RemoteScrollingCoordinatorProxyMac>(*protect(page()));
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

    auto& displayLinks = protect(page()->configuration())->processPool().displayLinks();
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

    // iOS does this from -[WKWebView _didCommitLayerTree:]. Without it the parameters stay zero on macOS, which
    // drops the layout viewport's minimum size and pins its origin to the top of the document, breaking sticky.
    if (page && usesDelegatedPageScaling())
        page->updateLayoutViewportParameters(mainFrameCommitData);

    // Re-apply on every commit, since the web process re-rasterizes during the gesture and those commits would
    // otherwise drop us back to the committed scale.
    if (m_transientZoomScale) {
        if (usesDelegatedPageScaling())
            applyDelegatedZoomToLayer(*m_transientZoomScale, m_transientZoomOriginInVisibleRect.value_or(FloatPoint { }));
        else
            applyTransientZoomToLayer();
    } else if (usesDelegatedPageScaling()) {
        // No gesture in flight, but still re-assert the scale every commit, using what the web process just
        // reported so that resets clear it.
        auto scaleToApply = mainFrameCommitData.pageScaleFactor;
        if (m_delegatedZoomCommittedScale) {
            // Keep showing the committed scale while the hold lasts, so an in-flight commit can't move the layer.
            scaleToApply = *m_delegatedZoomCommittedScale;

            if (++m_commitsSinceCommittingDelegatedZoom > maxCommitsToHoldDelegatedZoomScale) {
                m_delegatedZoomCommittedScale = { };
                m_commitsSinceCommittingDelegatedZoom = 0;
            }
        }

        // Only remove the override at (essentially) unity. It carries the whole visual scale, so calling 1.0043
        // near enough would leave the page permanently shrunk; zooming out commits exactly 1.0 anyway. Wait for
        // any snap-back to finish, since resetting to identity would cut the ease short.
        constexpr double unityScaleEpsilon = 0.0001;
        if (WTF::areEssentiallyEqual(scaleToApply, 1.0, unityScaleEpsilon)) {
            if (!hasDelegatedZoomSnapAnimation()) {
                m_delegatedZoomOriginInVisibleRect = { };
                removeDelegatedZoomFromLayer();
            }
        } else if (!hasDelegatedZoomSnapAnimation()) {
            // Skipped while easing, since this would replace the position override the snap-back is animating
            // and jump the content. animateDelegatedZoomSnapBack() re-asserts both when the ease is done.
            applyDelegatedZoomToLayer(scaleToApply, m_delegatedZoomOriginInVisibleRect.value_or(FloatPoint { }));
        }

        // The tree scales this layer's position by the same scale, so it has to agree with the transform, or
        // scroll offsets get scaled with nothing compensating for it. Use scaleToApply, not the commit value.
        if (CheckedPtr scrollingCoordinatorProxy = page->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->setDelegatedPageScaleFactor(scaleToApply);
    } else if (m_transactionIDAfterEndingTransientZoom && transactionID.greaterThanOrEqualSameProcess(*m_transactionIDAfterEndingTransientZoom)) {
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

static RetainPtr<CABasicAnimation> additiveTransientPositionAnimation(const FloatSize& offset)
{
    RetainPtr animation = fillFowardsAnimationWithKeyPathAndValue(@"position", [NSValue valueWithPoint:toFloatPoint(offset)]);
    [animation setAdditive:YES];
    return animation;
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
    // Instead of deriving a position relative to the scrolled contents layer,
    // we use this as an additive offset. Otherwise, transient zoom and scrolling
    // try to stomp over the same property in every frame.
    auto scrolledContentsCorrection = clipLayerPosition - transientClipLayerFrame.location();

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    auto animationWithScale = transientZoomTransformOverrideAnimation(transform);
    [layerForPageScale removeAnimationForKey:transientZoomAnimationKey];
    [layerForPageScale addAnimation:animationWithScale.get() forKey:transientZoomAnimationKey];
    [clipLayer removeAnimationForKey:transientClipPositionAnimationKey];
    [clipLayer removeAnimationForKey:transientClipSizeAnimationKey];
    [clipLayer addAnimation:transientPositionAnimation(transientClipLayerFrame.location()).get() forKey:transientClipPositionAnimationKey];
    [clipLayer addAnimation:transientSizeAnimation(transientClipLayerFrame.size()).get() forKey:transientClipSizeAnimationKey];
    [scrolledContentsLayer removeAnimationForKey:transientScrolledContentsPositionAnimationKey];
    [scrolledContentsLayer addAnimation:additiveTransientPositionAnimation(scrolledContentsCorrection).get() forKey:transientScrolledContentsPositionAnimationKey];
    END_BLOCK_OBJC_EXCEPTIONS

#if ENABLE(HORIZONTAL_BANNER_VIEW_OVERLAYS)
    if (RefPtr page = this->page()) {
        if (RefPtr pageClient = page->pageClient())
            pageClient->didUpdateTransientZoomStateForScrollPocket({ { *m_transientZoomScale, *m_transientZoomOriginInVisibleRect } });
    }
#endif
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

#if ENABLE(HORIZONTAL_BANNER_VIEW_OVERLAYS)
    if (RefPtr page = this->page()) {
        if (RefPtr pageClient = page->pageClient())
            pageClient->didUpdateTransientZoomStateForScrollPocket(std::nullopt);
    }
#endif
}

bool RemoteLayerTreeDrawingAreaProxyMac::usesDelegatedPageScaling() const
{
    RefPtr page = this->page();
    return page && page->delegatesScalingToUIProcess();
}

bool RemoteLayerTreeDrawingAreaProxyMac::ownsTransformOfLayer(WebCore::PlatformLayerIdentifier layerID) const
{
    // This is the one layer whose transform is the page scale instead of page content.
    if (!usesDelegatedPageScaling() || m_scrolledContentsLayerID != layerID)
        return false;

    // Not the same as a gesture being in flight; didCommitLayerTree() installs an override for the committed
    // scale too.
    return m_delegatedZoomAppliedScale.has_value();
}

bool RemoteLayerTreeDrawingAreaProxyMac::ownsPositionOfLayer(WebCore::PlatformLayerIdentifier layerID) const
{
    if (!usesDelegatedPageScaling() || m_scrolledContentsLayerID != layerID)
        return false;

    // The scrolling thread writes the right position here, but the web process's committed one is unscaled,
    // since RenderLayerCompositor uses -scrollPosition and normally has the page scale baked in below.
    return m_delegatedZoomAppliedScale.has_value();
}

void RemoteLayerTreeDrawingAreaProxyMac::applyDelegatedZoomToLayer(double scale, FloatPoint originInVisibleRect)
{
    // Scale the scrolled-contents layer like iOS does: inside the frame clipping layer so the viewport clip keeps
    // its size, and above the fixed and sticky layers so content at 0,0 lands in the corner.
    RetainPtr layerForPageScale = remoteLayerTreeHost().layerForID(m_scrolledContentsLayerID);
    if (!layerForPageScale)
        return;

    // A pure scale, so a point p ends up at (p - scrollPosition) * scale and fixed content laid out at the layout
    // viewport origin lands in the viewport corner at any scale. Anchoring is done by scrolling.
    TransformationMatrix transform;
    transform.scale(scale);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    // Set the model transform instead of animating it, because CA would intermittently ignore a fill-forwards
    // animation for a commit and present the model values. ownsTransformOfLayer() keeps this from being clobbered.
    [layerForPageScale setTransform:transform];
    m_delegatedZoomAppliedScale = scale;

    // Override the position in the same CA commit as the transform, or a frame with one and not the other jumps
    // by (scale - 1) * scroll. With no scrolling coordinator there's nothing to anchor to.
    if (RefPtr page = this->page(); page && page->scrollingCoordinatorProxy()) {
        auto scaledScrollPosition = scrolledContentsLayerPositionForScale(scale, scrollPositionAnchoringGestureOrigin(scale, originInVisibleRect));
        // An animation here, unlike the transform, since the scrolling thread rewrites this layer's model
        // position every commit and an animation wins over that.
        [layerForPageScale addAnimation:transientPositionAnimation(scaledScrollPosition).get() forKey:transientScrolledContentsPositionAnimationKey];
    }

    END_BLOCK_OBJC_EXCEPTIONS

#if ENABLE(HORIZONTAL_BANNER_VIEW_OVERLAYS)
    if (RefPtr page = this->page()) {
        if (RefPtr pageClient = page->pageClient()) {
            if (m_transientZoomScale || m_delegatedZoomCommittedScale)
                pageClient->didUpdateTransientZoomStateForScrollPocket({ { scale, originInVisibleRect } });
            else
                pageClient->didUpdateTransientZoomStateForScrollPocket(std::nullopt);
        }
    }
#endif
}

bool RemoteLayerTreeDrawingAreaProxyMac::hasDelegatedZoomSnapAnimation() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (RetainPtr layerForPageScale = remoteLayerTreeHost().layerForID(m_scrolledContentsLayerID))
        return !![layerForPageScale animationForKey:delegatedZoomSnapAnimationKey];
    END_BLOCK_OBJC_EXCEPTIONS
    return false;
}

// Eases the scale on the layer to the committed one, like the non-delegated path's snap back out of the elastic
// range. The transform is a model value here, so the animation needs an explicit fromValue; it expires on its
// own and leaves the model value alone.
void RemoteLayerTreeDrawingAreaProxyMac::animateDelegatedZoomSnapBack(double fromScale, double toScale, FloatPoint originInVisibleRect)
{
    RetainPtr layerForPageScale = remoteLayerTreeHost().layerForID(m_scrolledContentsLayerID);
    if (!layerForPageScale)
        return;

    TransformationMatrix fromTransform;
    fromTransform.scale(fromScale);
    TransformationMatrix toTransform;
    toTransform.scale(toScale);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    auto animation = DrawingArea::transientZoomSnapAnimationForKeyPath("transform"_s);
    // The model transform is already toScale, so this animation just covers the gap. It has to be removed on
    // completion, or animationForKey: keeps reporting it and hasDelegatedZoomSnapAnimation() never lets the
    // unity cleanup run.
    [animation setFillMode:kCAFillModeRemoved];
    [animation setRemovedOnCompletion:YES];
    [animation setFromValue:[NSValue valueWithCATransform3D:fromTransform]];
    [animation setToValue:[NSValue valueWithCATransform3D:toTransform]];
    [layerForPageScale removeAnimationForKey:delegatedZoomSnapAnimationKey];
    [layerForPageScale addAnimation:animation.get() forKey:delegatedZoomSnapAnimationKey];

    // The position is scaled by the page scale too, so easing only the transform drifts by
    // (toScale - presented) * scroll, which can be hundreds of pixels when the clamp fires while scrolled. Ease
    // it over the same curve, replacing the override applyDelegatedZoomToLayer() just installed. This one has to
    // stay fill-forwards, since the scrolling thread rewrites the model position every commit.
    if (RefPtr page = this->page(); page && page->scrollingCoordinatorProxy()) {
        auto fromPosition = scrolledContentsLayerPositionForScale(fromScale, scrollPositionAnchoringGestureOrigin(fromScale, originInVisibleRect));
        auto toPosition = scrolledContentsLayerPositionForScale(toScale, scrollPositionAnchoringGestureOrigin(toScale, originInVisibleRect));
        if (fromPosition != toPosition) {
            auto positionAnimation = DrawingArea::transientZoomSnapAnimationForKeyPath("position"_s);
            [positionAnimation setFromValue:[NSValue valueWithPoint:fromPosition]];
            [positionAnimation setToValue:[NSValue valueWithPoint:toPosition]];
            [layerForPageScale addAnimation:positionAnimation.get() forKey:transientScrolledContentsPositionAnimationKey];
        }
    }

    // Re-assert the settled transform and position once the ease has run, and leave the unity cleanup to the
    // next commit.
    if (RefPtr page = this->page()) {
        page->callAfterNextPresentationUpdate([page, toScale, originInVisibleRect] {
            RefPtr drawingArea = dynamicDowncast<RemoteLayerTreeDrawingAreaProxyMac>(page->drawingArea());
            if (!drawingArea || drawingArea->hasDelegatedZoomSnapAnimation())
                return;
            drawingArea->applyDelegatedZoomToLayer(toScale, originInVisibleRect);
        });
    }
    END_BLOCK_OBJC_EXCEPTIONS
}

FloatPoint RemoteLayerTreeDrawingAreaProxyMac::scrollPositionAnchoringGestureOrigin(double scale, FloatPoint originInVisibleRect) const
{
    RefPtr page = this->page();
    CheckedPtr scrollingCoordinatorProxy = page ? page->scrollingCoordinatorProxy() : nullptr;
    if (!scrollingCoordinatorProxy)
        return { };

    // A zoom that picked its own destination has no origin to anchor; see m_delegatedZoomTargetScrollPosition.
    if (m_delegatedZoomTargetScrollPosition)
        return constrainScrollPositionForScale(scale, *m_delegatedZoomTargetScrollPosition);

    auto currentScrollPosition = scrollingCoordinatorProxy->currentMainFrameScrollPosition();
    if (!m_delegatedZoomInitialScale || scale <= 0 || *m_delegatedZoomInitialScale <= 0)
        return currentScrollPosition;

    // Solve (p - scroll) * scale == origin for scroll, where p is the content point that was under the cursor
    // when the gesture started. Unscaled content coordinates, like the rest of the scrolling tree.
    auto anchorOffset = originInVisibleRect;
    anchorOffset.scale(1.0f / *m_delegatedZoomInitialScale - 1.0f / scale);

    // This positions the scrolled-contents layer and is also what sendVisibleContentRectUpdate() gives the
    // scrolling tree and the web process
    return constrainScrollPositionForScale(scale, m_delegatedZoomInitialScrollPosition + toFloatSize(anchorOffset));
}

FloatSize RemoteLayerTreeDrawingAreaProxyMac::unobscuredViewportSize() const
{
    RefPtr page = this->page();
    CheckedPtr scrollingCoordinatorProxy = page ? page->scrollingCoordinatorProxy() : nullptr;
    if (!scrollingCoordinatorProxy)
        return { };

    // Leaves out the space the scrollbars take, like ScrollView::sizeForUnobscuredContent() does in the web
    // process. The tree's value is in view coordinates, so it doesn't move with the zoom.
    auto sizeForVisibleContent = scrollingCoordinatorProxy->sizeForVisibleContent();
    auto viewportSize = sizeForVisibleContent.isEmpty() ? FloatSize { size() } : sizeForVisibleContent;

    auto obscuredInsets = scrollingCoordinatorProxy->obscuredContentInsets();
    viewportSize.expand(-(obscuredInsets.left() + obscuredInsets.right()), -(obscuredInsets.top() + obscuredInsets.bottom()));
    return viewportSize;
}

FloatPoint RemoteLayerTreeDrawingAreaProxyMac::constrainScrollPositionForScale(double scale, FloatPoint scrollPosition) const
{
    RefPtr page = this->page();
    CheckedPtr scrollingCoordinatorProxy = page ? page->scrollingCoordinatorProxy() : nullptr;
    if (!scrollingCoordinatorProxy || scale <= 0)
        return scrollPosition;

    // Unscaled, so the viewport grows as the scale falls. Idempotent, since the bounds come from the sizes alone.
    auto inverseScale = static_cast<float>(1.0 / scale);
    auto unobscuredViewportAtScale = FloatRect { scrollPosition, unobscuredViewportSize() * inverseScale };
    return ScrollableArea::constrainScrollPositionForOverhang(roundedIntRect(unobscuredViewportAtScale), roundedIntSize(scrollingCoordinatorProxy->totalContentsSize()), roundedIntPoint(scrollPosition), scrollingCoordinatorProxy->scrollOrigin(), scrollingCoordinatorProxy->headerHeight(), scrollingCoordinatorProxy->footerHeight());
}

// Has to match what ScrollingTreeFrameScrollingNodeMac::repositionScrollingLayers() writes for the same scale
// and scroll position, or the page jumps as the override comes and goes.
FloatPoint RemoteLayerTreeDrawingAreaProxyMac::scrolledContentsLayerPositionForScale(double scale, FloatPoint scrollPosition) const
{
    RefPtr page = this->page();
    CheckedPtr scrollingCoordinatorProxy = page ? page->scrollingCoordinatorProxy() : nullptr;
    if (!scrollingCoordinatorProxy)
        return { };

    auto rootContentsLayerPosition = LocalFrameView::positionForRootContentLayer(scrollPosition, scrollingCoordinatorProxy->scrollOrigin(), scrollingCoordinatorProxy->obscuredContentInsets(), scrollingCoordinatorProxy->headerHeight());
    return LocalFrameView::scrolledContentsLayerPositionForDelegatedPageScale(scrollPosition, scale, rootContentsLayerPosition);
}

String RemoteLayerTreeDrawingAreaProxyMac::delegatedZoomOverrideAsTextForTesting() const
{
    if (!m_delegatedZoomAppliedScale)
        return { };

    auto scale = *m_delegatedZoomAppliedScale;
    auto originInVisibleRect = m_transientZoomOriginInVisibleRect.value_or(m_delegatedZoomOriginInVisibleRect.value_or(FloatPoint { }));

    // The value both authorities are supposed to share: applyDelegatedZoomToLayer() turns it into the layer
    // position below, and sendVisibleContentRectUpdate() hands it to the scrolling tree and the web process.
    auto anchoringScrollPosition = roundedIntPoint(scrollPositionAnchoringGestureOrigin(scale, originInVisibleRect));

    // Read back off the layer rather than recomputed, so this is the position CA is presenting.
    auto layerPosition = IntPoint { };
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (RetainPtr layerForPageScale = remoteLayerTreeHost().layerForID(m_scrolledContentsLayerID)) {
        RetainPtr animation = dynamic_objc_cast<CABasicAnimation>([layerForPageScale animationForKey:transientScrolledContentsPositionAnimationKey]);
        if (RetainPtr value = dynamic_objc_cast<NSValue>([animation toValue]))
            layerPosition = roundedIntPoint(FloatPoint { [value pointValue] });
    }
    END_BLOCK_OBJC_EXCEPTIONS

    return makeString("scale "_s, FormattedNumber::fixedWidth(scale, 2),
        " anchoring scroll position ("_s, anchoringScrollPosition.x(), ", "_s, anchoringScrollPosition.y(),
        ") scrolled contents layer position ("_s, layerPosition.x(), ", "_s, layerPosition.y(), ')');
}


void RemoteLayerTreeDrawingAreaProxyMac::removeDelegatedZoomFromLayer()
{
    // Cleared first so ownsTransformOfLayer() and ownsPositionOfLayer() are right even if a removal throws.
    m_delegatedZoomAppliedScale = { };

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (RetainPtr layerForPageScale = remoteLayerTreeHost().layerForID(m_scrolledContentsLayerID)) {
        // The transform is a model value, so it needs resetting or the scale stays on the layer.
        [layerForPageScale setTransform:CATransform3DIdentity];

        // The position doesn't need resetting, since the scrolling thread owns and rewrites it. Removing the
        // animations is enough, including the non-delegated path's, which uses this same layer.
        [layerForPageScale removeAnimationForKey:transientZoomAnimationKey];
        [layerForPageScale removeAnimationForKey:transientZoomCommitAnimationKey];
        [layerForPageScale removeAnimationForKey:transientScrolledContentsPositionAnimationKey];
        [layerForPageScale removeAnimationForKey:delegatedZoomSnapAnimationKey];
    }

    if (RetainPtr clipLayer = remoteLayerTreeHost().layerForID(m_mainFrameClipLayerID)) {
        [clipLayer removeAnimationForKey:transientClipPositionAnimationKey];
        [clipLayer removeAnimationForKey:transientClipSizeAnimationKey];
    }
    END_BLOCK_OBJC_EXCEPTIONS

#if ENABLE(HORIZONTAL_BANNER_VIEW_OVERLAYS)
    if (RefPtr page = this->page()) {
        if (RefPtr pageClient = page->pageClient())
            pageClient->didUpdateTransientZoomStateForScrollPocket(std::nullopt);
    }
#endif
}

void RemoteLayerTreeDrawingAreaProxyMac::sendVisibleContentRectUpdate(double scale, IsStableState isStableState)
{
    RefPtr page = this->page();
    if (!page)
        return;

    CheckedPtr scrollingCoordinatorProxy = page->scrollingCoordinatorProxy();
    if (!scrollingCoordinatorProxy)
        return;

    // adjustLayersForLayoutViewport() below repositions layers under the scrolling tree lock, and that makes the
    // tree emit a scroll update on this thread, which comes back through updateLayoutViewportForScroll(). Set for
    // the whole function so the reentrant call bails out before it reads the tree and deadlocks.
    SetForScope sendingVisibleContentRectUpdate { m_isSendingVisibleContentRectUpdate, true };

    // Take the origin from the scrolling tree and let WebCore do the obscured inset math; the insets move with
    // the scroll position, so computing it here from the view size wouldn't work.
    auto visibleContentRect = scrollingCoordinatorProxy->computeVisibleContentRect();
    auto obscuredInsets = scrollingCoordinatorProxy->obscuredContentInsets();

    // The transform is a pure scale, so the gesture origin is anchored by moving the scroll position. The tree,
    // the layout viewport and the web process all have to agree on this value.
    auto anchoredScrollPosition = scrollPositionAnchoringGestureOrigin(scale, m_transientZoomOriginInVisibleRect.value_or(m_delegatedZoomOriginInVisibleRect.value_or(FloatPoint { })));

    // Size comes from the view, not from that rect. The web process recomputes the layout viewport from what we
    // send, so scaling it again every frame compounds and collapses the viewport mid-gesture.
    FloatSize viewportSize { size() };
    viewportSize.expand(-(obscuredInsets.left() + obscuredInsets.right()), -(obscuredInsets.top() + obscuredInsets.bottom()));

    auto inverseScale = static_cast<float>(1.0 / scale);

    // scrollPositionAnchoringGestureOrigin() already clamps what it synthesizes, so this only bounds the tree's
    // own live position.
    anchoredScrollPosition = constrainScrollPositionForScale(scale, anchoredScrollPosition);

    visibleContentRect.setLocation(anchoredScrollPosition);

    auto unobscuredContentRect = visibleContentRect;
    unobscuredContentRect.setSize(unobscuredViewportSize() * inverseScale);

    // The exposed rect is the full view, including the obscured areas the unobscured rect leaves out.
    auto exposedContentRect = unobscuredContentRect;
    exposedContentRect.setSize(FloatSize { size() } * inverseScale);

    auto layoutViewportRect = page->computeLayoutViewportRect(unobscuredContentRect, unobscuredContentRect, page->layoutViewportRect(), scale, LayoutViewportConstraint::ConstrainedToDocumentRect);

    // The view is unstable while the gesture is in flight, which keeps the web process from laying out
    // synchronously every frame. The update at the end of the gesture is stable.
    OptionSet<ViewStabilityFlag> viewStability;
    if (isStableState == IsStableState::No)
        viewStability.add(ViewStabilityFlag::ScrollViewInteracting);

    // Unscaled, since this is in scroll view coordinates. The resize event and window.inner* are sized from it
    // and shouldn't change as the page zooms, see LocalFrameView::sizeForResizeEvent().
    auto unobscuredRectInViewCoordinates = FloatRect { unobscuredContentRect.location(), viewportSize };

    VisibleContentRectUpdateInfo visibleContentRectUpdateInfo(
        exposedContentRect,
        unobscuredContentRect,
        FloatBoxExtent { }, // contentInsets
        unobscuredRectInViewCoordinates,
        unobscuredContentRect, // unobscuredContentRectRespectingInputViewBounds
        layoutViewportRect,
        obscuredInsets,
        FloatBoxExtent { }, // unobscuredSafeAreaInsets
        scale,
        viewStability,
        false, // isFirstUpdateForNewViewSize
        false, // allowShrinkToFit
        false, // enclosedInScrollableAncestorView
        false, // needsScrollend
        WebCore::VelocityData { },
        lastCommittedMainFrameLayerTreeTransactionID());

    LOG_WITH_STREAM(VisibleRects, stream << "RemoteLayerTreeDrawingAreaProxyMac::sendVisibleContentRectUpdate " << visibleContentRectUpdateInfo.dump());

    page->updateVisibleContentRects(visibleContentRectUpdateInfo, isStableState == IsStableState::Yes);

    // Push the layout viewport into the UI-process scrolling tree so fixed and sticky layers are placed against
    // the rect we just sent instead of lagging until the next commit. Read it back from the page, which
    // updateVisibleContentRects() has already updated, so it matches what the web process will compute.
    auto unconstrainedLayoutViewport = page->unconstrainedLayoutViewportRect();

    // Pass the tree's own scroll position; on macOS the tree owns it.
    page->adjustLayersForLayoutViewport(anchoredScrollPosition, unconstrainedLayoutViewport, scale);
}

void RemoteLayerTreeDrawingAreaProxyMac::updateLayoutViewportForScroll(IsStableState isStableState)
{
    if (!usesDelegatedPageScaling())
        return;

    // A magnify gesture already drives updates from adjustTransientZoom(), at its own scale.
    if (m_transientZoomScale)
        return;

    RefPtr page = this->page();
    if (!page)
        return;

    // This is the scale the layout viewport was computed against. Not
    // RemoteScrollingCoordinatorProxy::mainFrameScaleFactor(), which is frameScaleFactor() and stays 1 here.
    auto scale = page->displayedContentScale();

    // At unity the layout viewport is just the unobscured rect, and the web process keeps that up to date itself.
    constexpr double unityScaleEpsilon = 0.0001;
    if (scale <= 0 || WTF::areEssentiallyEqual(scale, 1.0, unityScaleEpsilon))
        return;

    CheckedPtr scrollingCoordinatorProxy = page->scrollingCoordinatorProxy();
    if (!scrollingCoordinatorProxy)
        return;

    if (m_isSendingVisibleContentRectUpdate || scrollingCoordinatorProxy->isCommittingScrollingTreeState())
        return;

    // Nothing has moved since the last push, so there is no new rect to send.
    auto scrollPosition = scrollingCoordinatorProxy->currentMainFrameScrollPosition();
    if (isStableState == IsStableState::No && m_lastScrollPositionPushedForLayoutViewport == scrollPosition && m_lastScalePushedForLayoutViewport == scale)
        return;

    m_lastScrollPositionPushedForLayoutViewport = scrollPosition;
    m_lastScalePushedForLayoutViewport = scale;

    sendVisibleContentRectUpdate(scale, isStableState);
}

void RemoteLayerTreeDrawingAreaProxyMac::adjustTransientZoom(double scale, FloatPoint originInLayerForPageScale, WebCore::FloatPoint originInVisibleRect)

{
    LOG_WITH_STREAM(ViewGestures, stream << "RemoteLayerTreeDrawingAreaProxyMac::adjustTransientZoom - scale " << scale << " originInLayerForPageScale " << originInLayerForPageScale << " originInVisibleRect " << originInVisibleRect);

    m_transientZoomScale = scale;
    m_transientZoomOriginInLayerForPageScale = originInLayerForPageScale;
    m_transientZoomOriginInVisibleRect = originInVisibleRect;

    if (usesDelegatedPageScaling()) {
        // A new gesture takes over from the previous hold, which would otherwise bring back a stale scale, and
        // from any snap-back still easing out of the last release.
        m_delegatedZoomCommittedScale = { };
        m_commitsSinceCommittingDelegatedZoom = 0;
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        if (RetainPtr layerForPageScale = remoteLayerTreeHost().layerForID(m_scrolledContentsLayerID))
            [layerForPageScale removeAnimationForKey:delegatedZoomSnapAnimationKey];
        END_BLOCK_OBJC_EXCEPTIONS

        // First frame of the gesture, so remember where we started for scrollPositionAnchoringGestureOrigin().
        if (!m_delegatedZoomInitialScale) {
            RefPtr page = this->page();
            CheckedPtr scrollingCoordinatorProxy = page ? page->scrollingCoordinatorProxy() : nullptr;
            m_delegatedZoomInitialScale = page ? page->pageScaleFactor() : 1;
            m_delegatedZoomInitialScrollPosition = scrollingCoordinatorProxy ? scrollingCoordinatorProxy->currentMainFrameScrollPosition() : FloatPoint { };
        }

        applyDelegatedZoomToLayer(scale, originInVisibleRect);
        sendVisibleContentRectUpdate(scale, IsStableState::No);
        return;
    }

    applyTransientZoomToLayer();

    // FIXME: Only send these messages as fast as the web process is responding to them.
    send(Messages::DrawingArea::AdjustTransientZoom(scale, originInLayerForPageScale));
}

void RemoteLayerTreeDrawingAreaProxyMac::commitDelegatedZoom(double scale, FloatPoint originInVisibleRect)
{
    RefPtr page = this->page();
    if (!page)
        return;

    auto rootScrollingNodeID = [&]() -> std::optional<WebCore::ScrollingNodeID> {
        CheckedPtr scrollingCoordinatorProxy = page->scrollingCoordinatorProxy();
        if (!scrollingCoordinatorProxy)
            return std::nullopt;
        auto nodeID = scrollingCoordinatorProxy->rootScrollingNodeID();
        if (nodeID)
            scrollingCoordinatorProxy->deferWheelEventTestCompletionForReason(nodeID, WheelEventTestMonitorDeferReason::CommittingTransientZoom);
        return nodeID;
    }();

    // The gesture goes into the elastic range, down to minMagnification * 0.75, and endMagnificationGesture()
    // clamps back to [min, max], so the layer can be showing a scale the committed one has to travel to. Ease
    // over that gap like the non-delegated path does.
    auto appliedScale = m_delegatedZoomAppliedScale;
    m_delegatedZoomOriginInVisibleRect = originInVisibleRect;
    applyDelegatedZoomToLayer(scale, originInVisibleRect);
    if (appliedScale && !WTF::areEssentiallyEqual(*appliedScale, scale))
        animateDelegatedZoomSnapBack(*appliedScale, scale, originInVisibleRect);

    // Hold this scale until the web process catches up. Commits already in flight carry the old pageScaleFactor,
    // and applying those would visibly step the scale backwards.
    m_delegatedZoomCommittedScale = scale;
    m_commitsSinceCommittingDelegatedZoom = 0;

    m_transientZoomScale = { };
    m_transientZoomOriginInLayerForPageScale = { };
    m_transientZoomOriginInVisibleRect = { };

    // The scale rides along on the stable visible content rect update. scalePageRelativeToScrollPosition() would
    // multiply in viewScaleFactor() again and bake the scale back into the render tree.
    sendVisibleContentRectUpdate(scale, IsStableState::Yes);

    // The gesture is over; the next one captures its own starting scale and scroll position.
    m_delegatedZoomInitialScale = { };
    m_delegatedZoomInitialScrollPosition = { };

    // Release the hold once the web process has drawn at the committed scale. Not gated on rootScrollingNodeID,
    // or a page with no scrolling node would keep the override forever.
    page->callAfterNextPresentationUpdate([page, scale] {
        RefPtr drawingArea = dynamicDowncast<RemoteLayerTreeDrawingAreaProxyMac>(page->drawingArea());
        if (!drawingArea)
            return;

        // A new gesture may have started in the meantime, in which case it owns the scale and
        // adjustTransientZoom() has already cleared this.
        if (!drawingArea->m_delegatedZoomCommittedScale || *drawingArea->m_delegatedZoomCommittedScale != scale)
            return;

        drawingArea->m_delegatedZoomCommittedScale = { };
        drawingArea->m_commitsSinceCommittingDelegatedZoom = 0;
    });

    if (!rootScrollingNodeID)
        return;

    page->callAfterNextPresentationUpdate([rootScrollingNodeID, page] {
        if (CheckedPtr scrollingCoordinatorProxy = page->scrollingCoordinatorProxy())
            scrollingCoordinatorProxy->removeWheelEventTestCompletionDeferralForReason(rootScrollingNodeID, WheelEventTestMonitorDeferReason::CommittingTransientZoom);
    });
}

void RemoteLayerTreeDrawingAreaProxyMac::commitTransientZoom(double scale, FloatPoint origin, std::optional<FloatPoint> targetScrollPosition)
{
    RefPtr page = this->page();
    if (!page)
        return;

    if (usesDelegatedPageScaling()) {
        // Our transform is a pure scale, so `origin`, a layer translation, can't move the page here, and it can't
        // be converted into a scroll position that would: ViewGestureController::scaledMagnificationOrigin() folds
        // m_visibleContentRect.location() into it, which is in unscaled content coordinates once the UI process
        // owns the page scale (FrameView::visibleContentScaleFactor()), while the anchor it's added to is in view
        // coordinates. A gesture is anchored from the origin adjustTransientZoom() tracked instead, and a zoom that
        // picked its own destination has to name it outright.
        m_delegatedZoomTargetScrollPosition = targetScrollPosition;
        commitDelegatedZoom(scale, m_transientZoomOriginInVisibleRect.value_or(FloatPoint { }));
        m_delegatedZoomTargetScrollPosition = { };
        return;
    }

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

    // From now until sendCommitTransientZoom() applies it, the page scale factor still describes
    // the scale from before the gesture, so we account for it and report this instead.
    m_committedTransientZoomScale = scale;

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

    m_committedTransientZoomScale = std::nullopt;

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
    m_needsDisplayRefreshCallbacksForDrawing = true;
    if (m_displayRefreshObserverID)
        return;

    // FIXME: as stated in the header, we should make m_displayID non-optional. An empty display ID
    // can cause presentation update callbacks for the page to be stuck forever.
    if (!m_displayID) {
        RefPtr webPageProxy = page();
        RELEASE_LOG_ERROR(DisplayLink, "%p [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i] RemoteLayerTreeDrawingAreaProxyMac::scheduleDisplayRefreshCallbacks(): page has no display ID", this, webPageProxy ? webPageProxy->identifier().toUInt64() : 0, webPageProxy ? webPageProxy->webPageIDInMainFrameProcess().toUInt64() : 0, webPageProxy ? webPageProxy->legacyMainFrameProcessID() : 0);
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
    m_needsDisplayRefreshCallbacksForDrawing = false;
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

void RemoteLayerTreeDrawingAreaProxyMac::windowScreenDidChange(PlatformDisplayID displayID)
{
    if (displayID == m_displayID)
        return;

    bool needsDisplayRefreshCallbacksForDrawing = m_needsDisplayRefreshCallbacksForDrawing;
    bool hadFullSpeedOberver = m_fullSpeedUpdateObserverID.has_value();
    if (hadFullSpeedOberver)
        removeObserver(m_fullSpeedUpdateObserverID);

    pauseDisplayRefreshCallbacks();

    RefPtr page = this->page();
    if (m_displayID && page)
        protect(page->scrollingCoordinatorProxy())->windowScreenWillChange();

    m_displayID = displayID;
    m_displayNominalFramesPerSecond = displayNominalFramesPerSecond();

    if (page)
        protect(page->scrollingCoordinatorProxy())->windowScreenDidChange(displayID, m_displayNominalFramesPerSecond);

    if (needsDisplayRefreshCallbacksForDrawing)
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
        protect(page->scrollingCoordinatorProxy())->displayDidRefresh(m_displayID.value_or(0));
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

    RetainPtr<CAContext> rootLayerContext = [protect(page->acceleratedCompositingRootLayer()) context];
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
    [CATransaction addCommitHandler:[callbackID, connection = WTF::move(connection)] () mutable {
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
