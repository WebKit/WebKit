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

#pragma once

#include "RemoteLayerTreeDrawingAreaProxy.h"

#if PLATFORM(MAC)

#include "DisplayLinkObserverID.h"
#include <WebCore/AnimationFrameRate.h>

namespace WebKit {

class DisplayLink;
class RemoteLayerTreeDisplayLinkClient;
class RemoteLayerTreeTransaction;
class RemoteScrollingCoordinatorProxy;
class RemoteScrollingCoordinatorTransaction;
class WebProcessPool;

class RemoteLayerTreeDrawingAreaProxyMac final : public RemoteLayerTreeDrawingAreaProxy {
friend class RemoteScrollingCoordinatorProxyMac;
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerTreeDrawingAreaProxyMac);
    WTF_MAKE_NONCOPYABLE(RemoteLayerTreeDrawingAreaProxyMac);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteLayerTreeDrawingAreaProxyMac);
public:
    static Ref<RemoteLayerTreeDrawingAreaProxyMac> create(WebPageProxy&, WebProcessProxy&);
    ~RemoteLayerTreeDrawingAreaProxyMac();

    void didRefreshDisplay() override;

    DisplayLink& displayLink();
    DisplayLink* existingDisplayLink();

    void NODELETE updateZoomTransactionID();
    std::optional<WebCore::PlatformLayerIdentifier> pageScalingLayerID() { return m_pageScalingLayerID.asOptional(); }
    std::optional<WebCore::PlatformLayerIdentifier> pageScrollingLayerID() { return m_pageScrollingLayerID.asOptional(); }
    std::optional<WebCore::PlatformLayerIdentifier> scrolledContentsLayerID() const { return m_scrolledContentsLayerID.asOptional(); }
    std::optional<WebCore::PlatformLayerIdentifier> mainFrameClipLayerID() const { return m_mainFrameClipLayerID.asOptional(); }

    // The zoom override currently installed on the scrolled-contents layer, empty when there is none
    String delegatedZoomOverrideAsTextForTesting() const;

private:
    RemoteLayerTreeDrawingAreaProxyMac(WebPageProxy&, WebProcessProxy&);

    WebCore::DelegatedScrollingMode delegatedScrollingMode() const override;
    std::unique_ptr<RemoteScrollingCoordinatorProxy> createScrollingCoordinatorProxy() const override;

    bool isRemoteLayerTreeDrawingAreaProxyMac() const override { return true; }

    void layoutBannerLayers(const RemoteLayerTreeTransaction&);

    void didCommitLayerTree(IPC::Connection&, const RemoteLayerTreeTransaction&, const RemoteScrollingCoordinatorTransaction&, const std::optional<MainFrameData>&, const TransactionID&) override;

    void adjustTransientZoom(double, WebCore::FloatPoint originInLayerForPageScale, WebCore::FloatPoint originInVisibleRect) override;
    void commitTransientZoom(double, WebCore::FloatPoint, std::optional<WebCore::FloatPoint>) override;
    std::optional<double> committedTransientZoomScale() const override { return m_committedTransientZoomScale; }

    void sendCommitTransientZoom(double, WebCore::FloatPoint, std::optional<WebCore::ScrollingNodeID>);

    void applyTransientZoomToLayer();
    void removeTransientZoomFromLayer();

    // With unified zoom the UI process owns the page scale and applies it below the scrolling layers, so the
    // viewport clip stays a fixed size and the scrolling tree keeps working in unscaled content coordinates.
    bool usesDelegatedPageScaling() const;
    // While a zoom override is installed on the scrolled-contents layer we own both its transform and its
    // position, so the property applier has to leave them alone.
    bool ownsTransformOfLayer(WebCore::PlatformLayerIdentifier) const final;
    bool ownsPositionOfLayer(WebCore::PlatformLayerIdentifier) const final;
    // The transform here is a pure scale, so there's no originInLayerForPageScale; the gesture origin is
    // anchored by the scroll position instead.
    void applyDelegatedZoomToLayer(double scale, WebCore::FloatPoint originInVisibleRect);
    void animateDelegatedZoomSnapBack(double fromScale, double toScale, WebCore::FloatPoint originInVisibleRect);
    bool hasDelegatedZoomSnapAnimation() const;
    void removeDelegatedZoomFromLayer();
    void commitDelegatedZoom(double scale, WebCore::FloatPoint originInVisibleRect);

    // The scroll position that keeps content under the gesture origin from moving at this scale. The zoom
    // transform sits above the scroll offset, so anchoring is done by scrolling, like UIScrollView does.
    WebCore::FloatPoint scrollPositionAnchoringGestureOrigin(double scale, WebCore::FloatPoint originInVisibleRect) const;

    // The unobscured viewport in view coordinates
    WebCore::FloatSize unobscuredViewportSize() const;

    // Clamps to what the document can reach at this scale
    WebCore::FloatPoint constrainScrollPositionForScale(double scale, WebCore::FloatPoint scrollPosition) const;

    // The scrolled-contents layer position the scrolling thread would write for this scale and scroll position.
    WebCore::FloatPoint scrolledContentsLayerPositionForScale(double scale, WebCore::FloatPoint scrollPosition) const;

    enum class IsStableState : bool { No, Yes };
    void sendVisibleContentRectUpdate(double scale, IsStableState);

    // Scrolling has to refresh the web process's layout viewport override, or fixed and sticky layers stay placed
    // against the rect from the last zoom. iOS does this from -[WKContentView didUpdateVisibleRect:].
    void updateLayoutViewportForScroll(IsStableState);

    void scheduleDisplayRefreshCallbacks() override;
    void pauseDisplayRefreshCallbacks() override;
    void setPreferredFramesPerSecond(IPC::Connection&, WebCore::FramesPerSecond) override;
    void windowScreenDidChange(WebCore::PlatformDisplayID) override;
    std::optional<WebCore::FramesPerSecond> displayNominalFramesPerSecond() override;

    void dispatchSetObscuredContentInsets() override;

    void colorSpaceDidChange() override;

    void viewIsBecomingVisible() final;
    void viewIsBecomingInvisible() final;

    void didChangeViewExposedRect() override;

    void removeObserver(std::optional<DisplayLinkObserverID>&);

    WTF::MachSendRight createFence() override;

    std::optional<WebCore::PlatformDisplayID> m_displayID; // Would be nice to make this non-optional, and ensure we always get one on creation.
    std::optional<WebCore::FramesPerSecond> m_displayNominalFramesPerSecond;
    WebCore::FramesPerSecond m_clientPreferredFramesPerSecond { WebCore::FullSpeedFramesPerSecond };

    std::optional<DisplayLinkObserverID> m_displayRefreshObserverID;
    std::optional<DisplayLinkObserverID> m_fullSpeedUpdateObserverID;
    const Ref<RemoteLayerTreeDisplayLinkClient> m_displayLinkClient;
    const WeakPtr<WebProcessPool> m_processPool;

    Markable<WebCore::PlatformLayerIdentifier> m_pageScalingLayerID;
    Markable<WebCore::PlatformLayerIdentifier> m_pageScrollingLayerID;
    Markable<WebCore::PlatformLayerIdentifier> m_scrolledContentsLayerID;
    Markable<WebCore::PlatformLayerIdentifier> m_mainFrameClipLayerID;

    bool m_shouldLogNextObserverChange { false };
    bool m_shouldLogNextDisplayRefresh { false };

    std::optional<WebCore::ScrollbarStyle> m_scrollbarStyle;

    std::optional<TransactionID> m_transactionIDAfterEndingTransientZoom;
    std::optional<double> m_transientZoomScale;
    std::optional<double> m_committedTransientZoomScale;
    std::optional<WebCore::FloatPoint> m_transientZoomOriginInLayerForPageScale;
    std::optional<WebCore::FloatPoint> m_transientZoomOriginInVisibleRect;

    // The gesture origin, kept so later commits can recompute the scroll position anchoring it. The scale comes
    // from each commit's MainFrameData::pageScaleFactor.
    std::optional<WebCore::FloatPoint> m_delegatedZoomOriginInVisibleRect;

    // Captured when a zoom gesture begins, to anchor the origin:
    // newScroll = initialScroll + origin * (1/initialScale - 1/scale).
    std::optional<double> m_delegatedZoomInitialScale;
    WebCore::FloatPoint m_delegatedZoomInitialScrollPosition;

    // Where the commit in progress was told to land, in unscaled content coordinates. Only zooms that pick their
    // own destination up front, like smart magnify, set it; a gesture has an origin to anchor instead, so this
    // stays empty and scrollPositionAnchoringGestureOrigin() does the work.
    std::optional<WebCore::FloatPoint> m_delegatedZoomTargetScrollPosition;

    // What updateLayoutViewportForScroll() last sent, so we don't bounce back a scroll update caused by our push.
    std::optional<WebCore::FloatPoint> m_lastScrollPositionPushedForLayoutViewport;
    std::optional<double> m_lastScalePushedForLayoutViewport;
    // Set while sendVisibleContentRectUpdate() holds the scrolling tree lock. The tree calls back into
    // updateLayoutViewportForScroll() from under the lock, where reading the tree again would deadlock.
    bool m_isSendingVisibleContentRectUpdate { false };

    // The scale committed at the end of a gesture, held until the web process has drawn at it. Commits still in
    // flight carry the old pageScaleFactor, and applying those would step the scale backwards.
    std::optional<double> m_delegatedZoomCommittedScale;
    // The scale installed on the scrolled-contents layer as a transform override. ownsTransformOfLayer() uses
    // this to tell whether an override exists.
    std::optional<double> m_delegatedZoomAppliedScale;
    unsigned m_commitsSinceCommittingDelegatedZoom { 0 };
    // In case the presentation update callback never arrives. Deliberately generous, since releasing the scale
    // too early is the bug we're guarding against.
    static constexpr unsigned maxCommitsToHoldDelegatedZoomScale { 60 };
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteLayerTreeDrawingAreaProxyMac)
    static bool isType(const WebKit::DrawingAreaProxy& proxy) { return proxy.isRemoteLayerTreeDrawingAreaProxyMac(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // #if PLATFORM(MAC)
