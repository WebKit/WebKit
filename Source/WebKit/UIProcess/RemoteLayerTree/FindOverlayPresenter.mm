/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "FindOverlayPresenter.h"

#if PLATFORM(MAC)

#import "FindOverlaySession.h"
#import "RemoteLayerTreeDrawingAreaProxyMac.h"
#import "RemoteLayerTreeHost.h"
#import "RemoteLayerTreeNode.h"
#import "RemoteLayerTreeTransaction.h"
#import "WebPageProxy.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/ImageBuffer.h>
#import <WebCore/NativeImage.h>
#import <WebCore/PathUtilities.h>
#import <wtf/TZoneMallocInlines.h>

// The veil drawing below reproduces FindController::drawRect exactly; keep the
// constants in sync with Source/WebKit/WebProcess/WebPage/FindController.cpp.
constexpr float findOverlayShadowOffsetX = 0;
constexpr float findOverlayShadowOffsetY = 0;
constexpr float findOverlayShadowBlurRadius = 1;
constexpr unsigned findOverlayIndicatorRadius = 3;
constexpr int findOverlayBorderWidth = 1;
constexpr auto findOverlayBackgroundColor = WebCore::SRGBA<uint8_t> { 26, 26, 26, 64 };
constexpr Seconds findOverlayFadeAnimationDuration { 200_ms };

@interface WKFindOverlayLayer : CALayer {
    Vector<WebCore::FloatRect> _matchRects;
    Vector<WebCore::FloatRect> _cutoutClearRects;
}
- (void)updateWithMatchRects:(Vector<WebCore::FloatRect>&&)matchRects cutoutClearRects:(Vector<WebCore::FloatRect>&&)cutoutClearRects coverageRect:(WebCore::FloatRect)coverageRect deviceScaleFactor:(CGFloat)deviceScaleFactor;
- (void)updateCutoutClearRects:(Vector<WebCore::FloatRect>&&)cutoutClearRects;
@end

@implementation WKFindOverlayLayer

- (instancetype)init
{
    self = [super init];
    if (!self)
        return nil;
    [self setName:@"Find overlay veil"];
    [self setAnchorPoint:CGPointZero];
    return self;
}

- (void)updateWithMatchRects:(Vector<WebCore::FloatRect>&&)matchRects cutoutClearRects:(Vector<WebCore::FloatRect>&&)cutoutClearRects coverageRect:(WebCore::FloatRect)coverageRect deviceScaleFactor:(CGFloat)deviceScaleFactor
{
    bool needsDisplay = false;

    if (self.contentsScale != deviceScaleFactor) {
        [self setContentsScale:deviceScaleFactor];
        needsDisplay = true;
    }

    // Anchoring the bounds origin at the coverage origin keeps drawing in the
    // root's contents coordinate space regardless of where the tile sits.
    CGRect coverage = coverageRect;
    if (!CGRectEqualToRect(self.frame, coverage)) {
        [self setPosition:coverage.origin];
        [self setBounds:coverage];
        needsDisplay = true;
    }

    if (_matchRects != matchRects || _cutoutClearRects != cutoutClearRects) {
        _matchRects = WTF::move(matchRects);
        _cutoutClearRects = WTF::move(cutoutClearRects);
        needsDisplay = true;
    }

    if (needsDisplay)
        [self repaint];
}

- (void)updateCutoutClearRects:(Vector<WebCore::FloatRect>&&)cutoutClearRects
{
    if (_cutoutClearRects == cutoutClearRects)
        return;
    _cutoutClearRects = WTF::move(cutoutClearRects);
    [self repaint];
}

- (void)repaint
{
    // Rasterize through an ImageBuffer rather than CA's drawInContext:
    // context, so the drawing environment (context conventions, shadow blur
    // scaling by the device scale, color space) matches the in-process
    // painter's layer backing store byte for byte.
    WebCore::FloatRect coverageRect = self.bounds;
    if (coverageRect.isEmpty()) {
        self.contents = nil;
        return;
    }

    // Two properties make this raster match the in-process painter's layer
    // backing byte for byte: the buffer is ACCELERATED, so the blurred drop
    // shadow renders through the CGStyle path rather than the software
    // ShadowBlur kernel an unaccelerated context would use (measured: 181 vs
    // 188 at the hole border's shadow dip); and the device scale is applied
    // as a user-space transform, so shadow blur radii scale with it.
    CGFloat deviceScaleFactor = self.contentsScale ?: 1;
    WebCore::FloatSize bufferSize = coverageRect.size();
    bufferSize.scale(deviceScaleFactor);
    RefPtr buffer = WebCore::ImageBuffer::create(bufferSize, WebCore::RenderingMode::Accelerated, WebCore::RenderingPurpose::Unspecified, 1, WebCore::ColorSpace::SRGB(), WebCore::PixelFormat::BGRA8);
    if (!buffer) {
        self.contents = nil;
        return;
    }

    auto& graphicsContext = buffer->context();
    graphicsContext.scale(deviceScaleFactor);
    graphicsContext.translate(-coverageRect.x(), -coverageRect.y());

    constexpr auto shadowColor = WebCore::Color::black.colorWithAlphaByte(128);

    WebCore::FloatRect rectFilter = coverageRect;
    rectFilter.inflate(findOverlayBorderWidth + findOverlayShadowBlurRadius);
    Vector<WebCore::FloatRect> rects;
    for (auto& rect : _matchRects) {
        if (rect.intersects(rectFilter))
            rects.append(rect);
    }

    // Draw the background.
    graphicsContext.fillRect(coverageRect, findOverlayBackgroundColor);

    Vector<WebCore::Path> whiteFramePaths = WebCore::PathUtilities::pathsWithShrinkWrappedRects(rects, findOverlayIndicatorRadius);

    {
        WebCore::GraphicsContextStateSaver stateSaver(graphicsContext);

        // Draw white frames around the holes.
        // We double the thickness because half of the stroke will be erased when we clear the holes.
        graphicsContext.setDropShadow({ { findOverlayShadowOffsetX, findOverlayShadowOffsetY }, findOverlayShadowBlurRadius, shadowColor, WebCore::ShadowRadiusMode::Default });
        graphicsContext.setStrokeColor(WebCore::Color::white);
        graphicsContext.setStrokeThickness(findOverlayBorderWidth * 2);
        for (auto& path : whiteFramePaths)
            graphicsContext.strokePath(path);

        graphicsContext.clearDropShadow();

        // Clear out the holes.
        graphicsContext.setCompositeOperation(WebCore::CompositeOperator::Clear);
        for (auto& path : whiteFramePaths)
            graphicsContext.fillPath(path);

        // Clear the full areas covered by child roots that dim themselves, so
        // every pixel is dimmed exactly once. No border, no shrink-wrap.
        for (auto& rect : _cutoutClearRects)
            graphicsContext.fillRect(rect);
    }

    RefPtr image = WebCore::ImageBuffer::sinkIntoNativeImage(WTF::move(buffer));
    self.contents = image ? (__bridge id)image->platformImage().get() : nil;
}

@end

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FindOverlayPresenter);

FindOverlayPresenter::FindOverlayPresenter(RemoteLayerTreeDrawingAreaProxyMac& drawingArea)
    : m_drawingArea(drawingArea)
{
}

FindOverlayPresenter::~FindOverlayPresenter()
{
    removeAllTiles();
}

WebPageProxy* FindOverlayPresenter::page() const
{
    return m_drawingArea->page();
}

void FindOverlayPresenter::findOverlaySessionDidChange()
{
    RefPtr page = this->page();
    RefPtr session = page ? page->findOverlaySession() : nullptr;

    if (!session) {
        // Dismissal. The web processes received the HideFindUI broadcast and
        // fade their reserved slots out over the shared 200ms (the slot's
        // opacity multiplies onto the tile), so the tiles keep their opacity
        // and are dropped once slot retirement reaches a commit. A single fade
        // owner keeps the ramp identical to the in-process painter's.
        m_veilVisible = false;
        return;
    }

    // A new find replaces the session before any result settles; keep the
    // current veil untouched until the replacement reaches a verdict so
    // incremental finds do not flicker.
    if (!session->settled())
        return;

    setVeilVisible(session->overlayShouldBeVisible());
}

void FindOverlayPresenter::didCommitTransaction(const RemoteLayerTreeTransaction& transaction)
{
    RefPtr page = this->page();
    RefPtr session = page ? page->findOverlaySession() : nullptr;

    // WebPageProxy::didCommitLayerTree has already folded this transaction's
    // payload into the session (validated against the committing connection,
    // with an absent payload for a known root erasing that root's entry), so
    // the store is current. All layer mutations below happen synchronously
    // inside the commit turn and join the implicit CATransaction that applies
    // the content's layer changes.
    if (session) {
        if (auto rootFrameID = transaction.rootFrameID())
            updateTileForRoot(*rootFrameID, transaction, *session);

        // Cross-root cutout inputs change on the CHILD's commits: a child's
        // first payload must clear the parent's cutout in this same turn, or a
        // static page double-dims the child's area with nothing to force a
        // recompute (and the reverse transition would under-dim).
        refreshCutoutClearRectsForAllTiles(*session);
    }

    removeDetachedTiles();
}

Vector<WebCore::FloatRect> FindOverlayPresenter::cutoutClearRectsForGeometry(const FindOverlaySession::RootGeometry& geometry, FindOverlaySession& session)
{
    // A child that publishes its own payload dims itself, so the parent clears
    // the child's area; a payload-less child stays covered by this tile's dim
    // as the steady state, keying the patch on payload presence, not timing.
    Vector<WebCore::FloatRect> cutoutClearRects;
    for (auto& childFrameRect : geometry.childRemoteFrameRects) {
        if (session.hasGeometryForRoot(childFrameRect.frameID))
            cutoutClearRects.append(childFrameRect.rect);
    }
    return cutoutClearRects;
}

bool FindOverlayPresenter::sessionHasParentCutoutCoveringRoot(FindOverlaySession& session, WebCore::FrameIdentifier rootFrameID)
{
    for (auto& geometry : session.rootGeometries().values()) {
        for (auto& childFrameRect : geometry.childRemoteFrameRects) {
            if (childFrameRect.frameID == rootFrameID)
                return true;
        }
    }
    return false;
}

void FindOverlayPresenter::updateTileForRoot(WebCore::FrameIdentifier rootFrameID, const RemoteLayerTreeTransaction& transaction, FindOverlaySession& session)
{
    // A root whose session entry disappeared between commits no longer dims
    // itself; drop its tile and let the parent's cutout dim patch cover it.
    auto* geometryPointer = session.geometryForRoot(rootFrameID);
    if (!geometryPointer) {
        removeTileForRoot(rootFrameID);
        return;
    }
    auto& geometry = *geometryPointer;

    // Once the web find state is retired, a straggling payload (for example
    // from a trailing find message that raced the retirement broadcast) must
    // not resurrect the veil.
    if (session.webFindStateRetired()) {
        removeTileForRoot(rootFrameID);
        return;
    }

    // The slot layer identity was validated against the committing process at
    // ingestion; a payload that arrived without a usable slot cannot host a
    // tile, so the parent's cutout dim patch covers this root instead.
    if (!geometry.veilSlotLayerID) {
        removeTileForRoot(rootFrameID);
        return;
    }

    RefPtr slotNode = m_drawingArea->remoteLayerTreeHost().nodeForID(geometry.veilSlotLayerID.asOptional());
    if (!slotNode) {
        removeTileForRoot(rootFrameID);
        return;
    }

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    auto& tile = m_tiles.ensure(rootFrameID, [&] {
        RetainPtr newTileLayer = adoptNS([[WKFindOverlayLayer alloc] init]);
        [newTileLayer setOpacity:m_veilVisible ? 1 : 0];
        // The verdict can settle before this root's first payload commit, so a
        // tile born visible still fades in over the shared duration, unless a
        // parent's cutout dim patch already covered this root's area, where
        // fading from the already-dimmed state would open an under-dim window;
        // snap instead and let the parent's clear land in the same turn.
        if (m_veilVisible && !sessionHasParentCutoutCoveringRoot(session, rootFrameID)) {
            RetainPtr fadeIn = [CABasicAnimation animationWithKeyPath:@"opacity"];
            [fadeIn setFromValue:@0];
            [fadeIn setToValue:@1];
            [fadeIn setDuration:findOverlayFadeAnimationDuration.seconds()];
            [fadeIn setTimingFunction:[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear]];
            [newTileLayer addAnimation:fadeIn.get() forKey:@"veilFadeIn"];
        }
        return Tile { WTF::move(newTileLayer), *geometry.veilSlotLayerID };
    }).iterator->value;
    tile.slotLayerID = *geometry.veilSlotLayerID;

    // Re-attach unconditionally: the slot never publishes children of its own,
    // so a commit cannot clobber the tile, but a recreated slot (new find after
    // dismissal, process swap) needs the tile moved under the new layer.
    RetainPtr slotLayer = slotNode->layer();
    if ([tile.layer superlayer] != slotLayer.get())
        [slotLayer addSublayer:tile.layer.get()];

    // Coverage is the visible rect plus roughly one viewport of total slop,
    // weighted toward the scroll axis. The tile rides the scrolled contents,
    // so pure scrolling needs no repositioning (and no repaint) until the
    // visible rect nears the coverage edge; only then is the tile re-centered
    // and repainted, amortizing the redraw cost over many scrolled commits.
    WebCore::FloatSize viewSize { m_drawingArea->size() };
    WebCore::FloatRect documentRect { WebCore::FloatPoint(), WebCore::FloatSize(transaction.contentsSize()) };
    WebCore::FloatRect visibleRect { WebCore::FloatPoint(transaction.scrollPosition()), viewSize };

    WebCore::FloatRect coverageRect { [tile.layer bounds] };
    WebCore::FloatRect guardRect = visibleRect;
    guardRect.inflateX(viewSize.width() / 16);
    guardRect.inflateY(viewSize.height() / 8);
    guardRect.intersect(documentRect);
    if (coverageRect.isEmpty() || !coverageRect.contains(guardRect)) {
        coverageRect = visibleRect;
        coverageRect.inflateX(viewSize.width() / 8);
        coverageRect.inflateY(viewSize.height() / 2);
        coverageRect.intersect(documentRect);
    }

    [tile.layer updateWithMatchRects:Vector<WebCore::FloatRect> { geometry.matchRectsInRootContentsCoordinates } cutoutClearRects:cutoutClearRectsForGeometry(geometry, session) coverageRect:coverageRect deviceScaleFactor:page() ? page()->deviceScaleFactor() : 1];

    [CATransaction commit];
}

void FindOverlayPresenter::refreshCutoutClearRectsForAllTiles(FindOverlaySession& session)
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    for (auto& [rootFrameID, tile] : m_tiles) {
        auto* geometry = session.geometryForRoot(rootFrameID);
        if (!geometry)
            continue;
        [tile.layer updateCutoutClearRects:cutoutClearRectsForGeometry(*geometry, session)];
    }
    [CATransaction commit];
}

static bool layerIsAttachedToLayer(CALayer *layer, CALayer *possibleAncestor)
{
    for (CALayer *ancestor = layer; ancestor; ancestor = ancestor.superlayer) {
        if (ancestor == possibleAncestor)
            return true;
    }
    return false;
}

void FindOverlayPresenter::removeDetachedTiles()
{
    RefPtr page = this->page();
    RefPtr session = page ? page->findOverlaySession() : nullptr;
    RetainPtr rootLayer = m_drawingArea->remoteLayerTreeHost().rootLayer();

    m_tiles.removeIf([&](auto& entry) {
        // A missing slot node means the root's process or the root itself went
        // away. A slot detached from the committed tree whose root the session
        // no longer tracks means the find was dismissed and the web process
        // retired the slot. (The slot CALayer and its host node can outlive
        // retirement, retained by an orphaned overlay container that never
        // flushes again, so attachment to the view's root layer is the
        // structural signal; a still-tracked root is kept through transient
        // detachment such as a hosted subtree awaiting its hosting link.)
        if (!m_drawingArea->remoteLayerTreeHost().nodeForID(entry.value.slotLayerID.asOptional())) {
            [entry.value.layer removeFromSuperlayer];
            return true;
        }
        bool sessionTracksRoot = session && session->hasGeometryForRoot(entry.key);
        if (!sessionTracksRoot && !layerIsAttachedToLayer([entry.value.layer superlayer], rootLayer.get())) {
            [entry.value.layer removeFromSuperlayer];
            return true;
        }
        return false;
    });
}

void FindOverlayPresenter::removeTileForRoot(WebCore::FrameIdentifier rootFrameID)
{
    if (auto tileIterator = m_tiles.find(rootFrameID); tileIterator != m_tiles.end()) {
        [tileIterator->value.layer removeFromSuperlayer];
        m_tiles.remove(tileIterator);
    }
}

void FindOverlayPresenter::removeAllTiles()
{
    for (auto& tile : m_tiles.values())
        [tile.layer removeFromSuperlayer];
    m_tiles.clear();
}

void FindOverlayPresenter::setVeilVisible(bool visible)
{
    if (m_veilVisible == visible)
        return;
    m_veilVisible = visible;

    if (!visible) {
        // A settled-not-visible verdict triggers the HideFindUI broadcast, so
        // the slot fade owns the ramp here too; tiles keep their opacity and
        // die with their slots.
        return;
    }

    [CATransaction begin];
    [CATransaction setAnimationDuration:findOverlayFadeAnimationDuration.seconds()];
    [CATransaction setAnimationTimingFunction:[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear]];
    for (auto& tile : m_tiles.values())
        [tile.layer setOpacity:1];
    [CATransaction commit];
}

} // namespace WebKit

#endif // PLATFORM(MAC)
