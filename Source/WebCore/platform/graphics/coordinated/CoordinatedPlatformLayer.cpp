/*
 * Copyright (C) 2024 Igalia S.L.
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

#include "config.h"
#include "CoordinatedPlatformLayer.h"

#if USE(COORDINATED_GRAPHICS) && !USE(TEXTURE_MAPPER)
#include "CoordinatedAnimatedBackingStoreClient.h"
#include "CoordinatedImageBackingStore.h"
#include "CoordinatedPlatformLayerBuffer.h"
#include "CoordinatedPlatformLayerBufferHolePunch.h"
#include "CoordinatedPlatformLayerBufferVideo.h"
#include "CoordinatedTileBuffer.h"
#include "CoordinatedTileManager.h"
#include "GraphicsContext.h"
#include "GraphicsLayerCoordinated.h"
#include "NativeImage.h"
#include "SkiaCompositingLayer.h"
#include <wtf/MainThread.h>

namespace WebCore {

Ref<CoordinatedPlatformLayer> CoordinatedPlatformLayer::create(Client& client)
{
    return adoptRef(*new CoordinatedPlatformLayer(&client));
}

Ref<CoordinatedPlatformLayer> CoordinatedPlatformLayer::create()
{
    return adoptRef(*new CoordinatedPlatformLayer(nullptr));
}

CoordinatedPlatformLayer::CoordinatedPlatformLayer(Client* client)
    : m_client(client)
    , m_id(PlatformLayerIdentifier::generate())
    , m_threadSafeGrContext(m_client ? m_client->tileManager().threadSafeGrContext() : nullptr)
{
    ASSERT(isMainThread());
}

CoordinatedPlatformLayer::~CoordinatedPlatformLayer() = default;

void CoordinatedPlatformLayer::setOwner(GraphicsLayerCoordinated* owner)
{
    assertIsMainThread();
    if (m_owner == owner)
        return;

    m_owner = owner;
    if (!m_client)
        return;

    if (m_owner)
        m_client->attachLayer(*this);
    else {
        purgeBackingStores();
        m_client->detachLayer(*this);
    }
}

GraphicsLayerCoordinated* CoordinatedPlatformLayer::owner() const
{
    assertIsMainThread();
    return m_owner;
}

CoordinatedTileManager& CoordinatedPlatformLayer::tileManager() const
{
    assertIsMainThread();
    ASSERT(m_client);

    return m_client->tileManager();
}

SkiaCompositingLayer& CoordinatedPlatformLayer::ensureTarget()
{
    ASSERT(!isMainThread());
    if (!m_target)
        m_target = SkiaCompositingLayer::create();
#if ENABLE(DAMAGE_TRACKING)
    m_target->setDamagePropagationEnabled(m_damagePropagationEnabled);
#endif
    return *m_target;
}

static bool shouldReleaseBuffer(CoordinatedPlatformLayerBuffer* buffer)
{
#if ENABLE(VIDEO)
    // Do not release hole punch buffers early. See https://bugs.webkit.org/show_bug.cgi?id=267322.
    if (is<CoordinatedPlatformLayerBufferHolePunch>(buffer))
        return false;
#else
    UNUSED_PARAM(buffer);
#endif

    return true;
}

void CoordinatedPlatformLayer::invalidateTarget()
{
    ASSERT(!isMainThread());
    if (m_target && !shouldReleaseBuffer(m_target->contentsBuffer())) {
        Locker locker { m_videoContents.lock };
        if (!m_videoContents.buffer.has_value())
            m_videoContents.buffer = m_target->takeContentsBuffer();
    }

    {
        Locker locker { m_committedRenderingUpdatesLock };
        m_committedRenderingUpdates.clear();
    }

    if (m_target) {
        m_target->invalidate();
        m_target = nullptr;
    }
}

void CoordinatedPlatformLayer::invalidateClient()
{
    ASSERT(isMainThread());
    purgeBackingStores();
    m_client = nullptr;
}

void CoordinatedPlatformLayer::notifyCompositionRequired()
{
    if (!m_client)
        return;
    m_client->notifyCompositionRequired();
}

void CoordinatedPlatformLayer::notifyScrollingPositionChanged()
{
    if (!m_client)
        return;
    m_client->didChangeLayerPendingScrollingPosition(*this);
}

void CoordinatedPlatformLayer::addPendingChange(Change change)
{
    assertIsMainThread();
    if (m_pendingChanges.isEmpty())
        notifyCompositionRequired();
    m_pendingChanges.add(change);
}

void CoordinatedPlatformLayer::setPosition(const FloatPoint& position)
{
    assertIsMainThread();
    assertIsHeld(m_lock);
    if (m_position == position)
        return;

    m_position = position;
    addPendingChange(Change::Position);
    {
        Locker locker { m_scrollingPosition.lock };
        m_scrollingPosition.currentPosition = std::nullopt;
    }
}

void CoordinatedPlatformLayer::setPositionForScrolling(const FloatPoint& position)
{
    Locker locker { m_scrollingPosition.lock };
    m_scrollingPosition.pendingPosition = position;
    if (!std::exchange(m_scrollingPosition.didNotifyPositionChange, true))
        notifyScrollingPositionChanged();
}

FloatPoint CoordinatedPlatformLayer::effectivePositionForScrolling() const
{
    assertIsHeld(m_lock);
    Locker locker { m_scrollingPosition.lock };
    return m_scrollingPosition.currentPosition.value_or(m_position);
}

void CoordinatedPlatformLayer::setTopLeftPositionForScrolling(const FloatPoint& position)
{
    FloatPoint newPosition;
    {
        Locker locker { m_lock };
        newPosition = { position.x() + m_anchorPoint.x() * m_size.width(), position.y() + m_anchorPoint.y() * m_size.height() };
    }
    setPositionForScrolling(newPosition);
}

FloatPoint CoordinatedPlatformLayer::topLeftPositionForScrolling()
{
    Locker locker { m_lock };
    return effectivePositionForScrolling() - toFloatSize(m_anchorPoint.xy()) * m_size;
}

void CoordinatedPlatformLayer::setBoundsOrigin(const FloatPoint& origin)
{
    assertIsMainThread();
    assertIsHeld(m_lock);
    if (m_boundsOrigin == origin)
        return;

    m_boundsOrigin = origin;
    addPendingChange(Change::BoundsOrigin);
    {
        Locker locker { m_scrollingPosition.lock };
        m_scrollingPosition.currentBoundsOrigin = std::nullopt;
    }
}

void CoordinatedPlatformLayer::setBoundsOriginForScrolling(const FloatPoint& origin)
{
    Locker locker { m_scrollingPosition.lock };
    m_scrollingPosition.pendingBoundsOrigin = origin;
    if (!std::exchange(m_scrollingPosition.didNotifyPositionChange, true))
        notifyScrollingPositionChanged();
}

FloatPoint CoordinatedPlatformLayer::effectiveBoundsOriginForScrolling() const
{
    assertIsHeld(m_lock);
    Locker locker { m_scrollingPosition.lock };
    return m_scrollingPosition.currentBoundsOrigin.value_or(m_boundsOrigin);
}

void CoordinatedPlatformLayer::setAnchorPoint(const FloatPoint3D& point)
{
    assertIsMainThread();
    assertIsHeld(m_lock);
    if (m_anchorPoint == point)
        return;

    m_anchorPoint = point;
    addPendingChange(Change::AnchorPoint);
}

const FloatPoint3D& CoordinatedPlatformLayer::anchorPoint() const
{
    assertIsHeld(m_lock);
    return m_anchorPoint;
}

void CoordinatedPlatformLayer::setSize(const FloatSize& size)
{
    assertIsMainThread();
    assertIsHeld(m_lock);
    if (m_size == size)
        return;

    m_size = size;
    addPendingChange(Change::Size);
}

const FloatSize& CoordinatedPlatformLayer::size() const
{
    assertIsHeld(m_lock);
    return m_size;
}

FloatRect CoordinatedPlatformLayer::bounds() const
{
    assertIsHeld(m_lock);
    return FloatRect({ }, m_size);
}

void CoordinatedPlatformLayer::setTransform(const TransformationMatrix& matrix)
{
    assertIsMainThread();
    assertIsHeld(m_lock);
    if (m_transform == matrix)
        return;

    m_transform = matrix;
    addPendingChange(Change::Transform);
}

const TransformationMatrix& CoordinatedPlatformLayer::transform() const
{
    assertIsHeld(m_lock);
    return m_transform;
}

void CoordinatedPlatformLayer::setChildrenTransform(const TransformationMatrix& matrix)
{
    assertIsMainThread();
    if (m_childrenTransform == matrix)
        return;

    m_childrenTransform = matrix;
    addPendingChange(Change::ChildrenTransform);
}

void CoordinatedPlatformLayer::didUpdateLayerTransform()
{
    assertIsMainThread();
    m_needsTilesUpdate = true;
}

void CoordinatedPlatformLayer::setVisibleRect(const FloatRect& visibleRect)
{
    assertIsMainThread();
    if (m_visibleRect == visibleRect)
        return;

    m_visibleRect = visibleRect;
}

void CoordinatedPlatformLayer::setTransformedVisibleRect(IntRect&& transformedVisibleRect)
{
    assertIsMainThread();
    if (m_transformedVisibleRect == transformedVisibleRect)
        return;

    m_transformedVisibleRect = WTF::move(transformedVisibleRect);
    m_needsTilesUpdate = true;
}

#if ENABLE(SCROLLING_THREAD)
void CoordinatedPlatformLayer::setScrollingNodeID(std::optional<ScrollingNodeID> nodeID)
{
    assertIsMainThread();
    assertIsHeld(m_lock);
    m_scrollingNodeID = nodeID;
}

const Markable<ScrollingNodeID>& CoordinatedPlatformLayer::scrollingNodeID() const
{
    assertIsHeld(m_lock);
    return m_scrollingNodeID;
}
#endif

void CoordinatedPlatformLayer::setDrawsContent(bool drawsContent)
{
    assertIsMainThread();
    m_drawsContent = drawsContent;
}

void CoordinatedPlatformLayer::setMasksToBounds(bool masksToBounds)
{
    assertIsMainThread();
    assertIsHeld(m_lock);
    if (m_masksToBounds == masksToBounds)
        return;

    m_masksToBounds = masksToBounds;
    addPendingChange(Change::MasksToBounds);
    damageWholeLayer();
}

bool CoordinatedPlatformLayer::masksToBounds() const
{
    assertIsHeld(m_lock);
    return m_masksToBounds;
}

void CoordinatedPlatformLayer::setPreserves3D(bool preserves3D)
{
    assertIsMainThread();
    if (m_preserves3D == preserves3D)
        return;

    m_preserves3D = preserves3D;
    addPendingChange(Change::Preserves3D);
}

void CoordinatedPlatformLayer::setBackfaceVisibility(bool backfaceVisibility)
{
    assertIsMainThread();
    if (m_backfaceVisibility == backfaceVisibility)
        return;

    m_backfaceVisibility = backfaceVisibility;
    addPendingChange(Change::BackfaceVisibility);
}

void CoordinatedPlatformLayer::setBackgroundColor(const Color& backgroundColor)
{
    assertIsMainThread();
    if (m_backgroundColor == backgroundColor)
        return;

    m_backgroundColor = backgroundColor;
    addPendingChange(Change::BackgroundColor);
}

void CoordinatedPlatformLayer::setOpacity(float opacity)
{
    assertIsMainThread();
    if (m_opacity == opacity)
        return;

    m_opacity = opacity;
    addPendingChange(Change::Opacity);
}

void CoordinatedPlatformLayer::setBlendMode(BlendMode blendMode)
{
    assertIsMainThread();
    if (m_blendMode == blendMode)
        return;

    m_blendMode = blendMode;
    addPendingChange(Change::BlendMode);
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setContentsVisible(bool contentsVisible)
{
    assertIsMainThread();
    if (m_contentsVisible == contentsVisible)
        return;

    m_contentsVisible = contentsVisible;
    addPendingChange(Change::ContentsVisible);
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setContentsOpaque(bool contentsOpaque)
{
    assertIsMainThread();
    if (m_contentsOpaque == contentsOpaque)
        return;

    m_contentsOpaque = contentsOpaque;
    addPendingChange(Change::ContentsOpaque);
    // FIXME: request a full repaint?
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setContentsRect(const FloatRect& contentsRect)
{
    assertIsMainThread();
    if (m_contentsRect == contentsRect)
        return;

    m_contentsRect = contentsRect;
    addPendingChange(Change::ContentsRect);
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setContentsRectClipsDescendants(bool contentsRectClipsDescendants)
{
    assertIsMainThread();
    if (m_contentsRectClipsDescendants == contentsRectClipsDescendants)
        return;

    m_contentsRectClipsDescendants = contentsRectClipsDescendants;
    addPendingChange(Change::ContentsRectClipsDescendants);
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setContentsClippingRect(const FloatRoundedRect& contentsClippingRect)
{
    assertIsMainThread();
    if (m_contentsClippingRect == contentsClippingRect)
        return;

    m_contentsClippingRect = contentsClippingRect;
    addPendingChange(Change::ContentsClippingRect);
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setContentsClipShapePath(const Path& path)
{
    assertIsMainThread();
    if (m_contentsClipShapePath.definitelyEqual(path))
        return;

    m_contentsClipShapePath = path;
    addPendingChange(Change::ContentsClipShapePath);
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setContentsScale(float contentsScale)
{
    assertIsMainThread();
    if (m_contentsScale == contentsScale)
        return;

    m_contentsScale = contentsScale;
    m_needsTilesUpdate = true;
}

float CoordinatedPlatformLayer::contentsScale() const
{
    return m_contentsScale;
}

void CoordinatedPlatformLayer::setContentsBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&& buffer, std::optional<Damage>&& dirtyRegion)
{
    assertIsMainThread();
    m_contentsBuffer = WTF::move(buffer);
    addPendingChange(Change::ContentsBuffer);

#if ENABLE(DAMAGE_TRACKING)
    if (dirtyRegion)
        addDamage(WTF::move(*dirtyRegion));
    else
        damageWholeLayer();
#else
    UNUSED_PARAM(dirtyRegion);
#endif
}

void CoordinatedPlatformLayer::setScrollbarContentsBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&& buffer, const FloatRect& rect)
{
    ASSERT(buffer);
    {
        Locker locker { m_scrollbarContents.lock };
        m_scrollbarContents.buffer = WTF::move(buffer);
        m_scrollbarContents.rect = rect;
    }

    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::clearScrollbarContentsBuffer()
{
    {
        Locker locker { m_scrollbarContents.lock };
        m_scrollbarContents.buffer = nullptr;
        m_scrollbarContents.rect = std::nullopt;
    }

    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setVideoContentsBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&& buffer, RequireComposition requireComposition)
{
    ASSERT(buffer);
    {
        Locker locker { m_videoContents.lock };
        m_videoContents.buffer = WTF::move(buffer);
    }

    if (requireComposition == RequireComposition::Yes)
        requestComposition(CompositionReason::VideoFrame);
    else
        notifyCompositionRequired();
}

void CoordinatedPlatformLayer::clearVideoContentsBuffer()
{
    {
        Locker locker { m_videoContents.lock };
        m_videoContents.buffer = nullptr;
    }

    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::discardVideoContentsBuffer()
{
    Locker locker { m_videoContents.lock };
    m_videoContents.buffer = std::nullopt;
}

#if ENABLE(VIDEO) && USE(GSTREAMER_GL)
void CoordinatedPlatformLayer::replaceCurrentContentsBufferWithCopy()
{
    if (!m_target)
        return;

    auto* buffer = m_target->contentsBuffer();
    if (!buffer)
        return;

    {
        Locker locker { m_videoContents.lock };
        m_videoContents.buffer = std::nullopt;
    }

    auto copiedBuffer = is<CoordinatedPlatformLayerBufferVideo>(*buffer) ? downcast<CoordinatedPlatformLayerBufferVideo>(*buffer).copyBuffer() : nullptr;
    m_target->setContentsBuffer(WTF::move(copiedBuffer));
}
#endif

void CoordinatedPlatformLayer::setContentsImage(NativeImage* image)
{
    assertIsMainThread();
    if (image) {
        if (m_imageBackingStore && m_imageBackingStore->isSameNativeImage(*image))
            return;

        ASSERT(m_client);
        m_imageBackingStore = m_client->imageBackingStore(Ref { *image });
    } else {
        if (!m_imageBackingStore)
            return;
        m_imageBackingStore = nullptr;
    }
    addPendingChange(Change::ContentsImage);
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setContentsColor(const Color& color)
{
    assertIsMainThread();
    if (m_contentsColor == color)
        return;

    m_contentsColor = color;
    addPendingChange(Change::ContentsColor);
}

void CoordinatedPlatformLayer::setContentsTileSize(const FloatSize& contentsTileSize)
{
    assertIsMainThread();
    if (m_contentsTileSize == contentsTileSize)
        return;

    m_contentsTileSize = contentsTileSize;
    addPendingChange(Change::ContentsTiling);
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setContentsTilePhase(const FloatSize& contentsTilePhase)
{
    assertIsMainThread();
    if (m_contentsTilePhase == contentsTilePhase)
        return;

    m_contentsTilePhase = contentsTilePhase;
    addPendingChange(Change::ContentsTiling);
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setDirtyRegion(Damage&& damage)
{
    assertIsMainThread();
    m_dirtyRegion = damage.rects();

#if ENABLE(DAMAGE_TRACKING)
    addDamage(WTF::move(damage));
#endif
}

#if ENABLE(DAMAGE_TRACKING)
void CoordinatedPlatformLayer::addDamage(Damage&& damage)
{
    assertIsMainThread();
    if (!m_damage)
        m_damage = WTF::move(damage);
    else
        m_damage->add(damage);
    addPendingChange(Change::Damage);
}
#endif

void CoordinatedPlatformLayer::damageWholeLayer()
{
#if ENABLE(DAMAGE_TRACKING)
    // An empty Damage rejects everything added to it later, so it must never become the layer's damage.
    if (!m_damagePropagationEnabled || sizeOnMainThread().isEmpty())
        return;

    addDamage(Damage { sizeOnMainThread(), Damage::Mode::Full });
#endif
}

void CoordinatedPlatformLayer::setFilters(const FilterOperations& filters)
{
    assertIsMainThread();
    if (m_filters == filters)
        return;

    m_filters = filters;
    addPendingChange(Change::Filters);
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setMask(CoordinatedPlatformLayer* mask)
{
    assertIsMainThread();
    assertIsHeld(m_lock);
    if (m_mask == mask)
        return;

    m_mask = mask;
    addPendingChange(Change::Mask);
    damageWholeLayer();
}

CoordinatedPlatformLayer* CoordinatedPlatformLayer::mask() const
{
    assertIsHeld(m_lock);
    return m_mask;
}

void CoordinatedPlatformLayer::setReplica(CoordinatedPlatformLayer* replica)
{
    assertIsMainThread();
    if (m_replica == replica)
        return;

    m_replica = replica;
    addPendingChange(Change::Replica);
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setBackdropFilters(const FilterOperations& filters)
{
    assertIsMainThread();
    if (m_backdropFilters == filters)
        return;

    m_backdropFilters = filters;
    addPendingChange(Change::Backdrop);
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setBackdropRect(const FloatRoundedRect& backdropRect)
{
    assertIsMainThread();
    if (m_backdropRect == backdropRect)
        return;

    m_backdropRect = backdropRect;
    addPendingChange(Change::BackdropRect);
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setBackdropShapePath(const Path& path)
{
    assertIsMainThread();
    if (m_backdropShapePath.definitelyEqual(path))
        return;

    m_backdropShapePath = path;
    addPendingChange(Change::BackdropShapePath);
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setIsBackdropRoot(bool isBackdropRoot)
{
    assertIsMainThread();
    if (m_isBackdropRoot == isBackdropRoot)
        return;

    m_isBackdropRoot = isBackdropRoot;
    addPendingChange(Change::BackdropRoot);
}

void CoordinatedPlatformLayer::setAnimations(const AcceleratedAnimations& animations)
{
    assertIsMainThread();
    m_animations = animations;
    addPendingChange(Change::Animations);
}

RefPtr<CoordinatedPlatformLayer> CoordinatedPlatformLayer::parent() const
{
    assertIsHeld(m_lock);
    return m_parent;
}

void CoordinatedPlatformLayer::setChildren(Vector<Ref<CoordinatedPlatformLayer>>&& children)
{
    assertIsMainThread();
    assertIsHeld(m_lock);
    if (m_children == children)
        return;

    while (!m_children.isEmpty()) {
        auto child = m_children.takeLast();
        Locker childLocker { child->m_lock };
        child->m_parent = nullptr;
    }

    m_children = WTF::move(children);

    for (auto& child : m_children) {
        Locker childLocker { child->m_lock };
        child->removeFromParent();
        child->m_parent = this;
    }

    addPendingChange(Change::Children);
}

void CoordinatedPlatformLayer::removeFromParent()
{
    assertIsHeld(m_lock);
    RefPtr parent = std::exchange(m_parent, nullptr);
    if (!parent)
        return;

    Locker parentLocker { parent->m_lock };

    parent->m_children.removeFirstMatching([this](auto& layer) {
        return layer.ptr() == this;
    });
}

const Vector<Ref<CoordinatedPlatformLayer>>& CoordinatedPlatformLayer::children() const
{
    assertIsHeld(m_lock);
    return m_children;
}

void CoordinatedPlatformLayer::setEventRegion(const EventRegion& eventRegion)
{
    assertIsMainThread();
    assertIsHeld(m_lock);
    m_eventRegion = eventRegion;
}

const EventRegion& CoordinatedPlatformLayer::eventRegion() const
{
    assertIsHeld(m_lock);
    return m_eventRegion;
}

void CoordinatedPlatformLayer::setClipPath(const Path& path, WindRule windRule)
{
    assertIsMainThread();
    m_clipPath.path = path;
    m_clipPath.windRule = windRule;
    addPendingChange(Change::ClipPath);
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setDebugBorder(Color&& borderColor, float borderWidth)
{
    assertIsMainThread();
    if (m_debugBorderColor == borderColor && m_debugBorderWidth == borderWidth)
        return;

    m_debugBorderColor = WTF::move(borderColor);
    m_debugBorderWidth = borderWidth;
    addPendingChange(Change::DebugIndicators);
}

void CoordinatedPlatformLayer::setShowRepaintCounter(bool showRepaintCounter)
{
    assertIsMainThread();
    if ((m_repaintCount != -1 && showRepaintCounter) || (m_repaintCount == -1 && !showRepaintCounter))
        return;

    m_repaintCount = showRepaintCounter ? m_owner->repaintCount() : -1;
    addPendingChange(Change::DebugIndicators);
}

bool CoordinatedPlatformLayer::needsBackingStore() const
{
    assertIsMainThread();
    if (!m_owner)
        return false;

    if (!m_drawsContent || !m_contentsVisible || sizeOnMainThread().isEmpty())
        return false;

    // If the CSS opacity value is 0 and there's no animation over the opacity property, the layer is invisible.
    if (!m_opacity && !m_animations.hasActiveAnimationsOfType(AnimatedProperty::Opacity))
        return false;

    // Check if there's a filter that sets the opacity to zero.
    bool hasOpacityZeroFilter = std::ranges::any_of(m_filters, [](auto& operation) {
        return operation->type() == FilterOperation::Type::Opacity && !downcast<BasicComponentTransferFilterOperation>(operation).amount();
    });

    return !hasOpacityZeroFilter;
}

void CoordinatedPlatformLayer::updateBackingStore()
{
    assertIsMainThread();

    if (!m_backingStoreProxy || (m_dirtyRegion.isEmpty() && !m_needsTilesUpdate))
        return;

    Damage damage(sizeOnMainThread(), Damage::Mode::Rectangles);
    bool shouldCreateDestroyTiles = m_needsTilesUpdate;
    auto tilesUpdate = m_backingStoreProxy->updateIfNeeded(m_transformedVisibleRect, sizeOnMainThread(), m_visibleRect, m_contentsScale, m_contentsOpaque, shouldCreateDestroyTiles, m_dirtyRegion, damage, *this);
    m_dirtyRegion.clear();
    m_needsTilesUpdate = false;

    if (!tilesUpdate)
        return;

#if ENABLE(DAMAGE_TRACKING)
    addDamage(WTF::move(damage));
#endif
    addPendingChange(Change::BackingStoreTiles);

    if (m_repaintCount != -1 && !tilesUpdate->tilesToUpdate().isEmpty()) {
        m_repaintCount = m_owner->incrementRepaintCount();
        addPendingChange(Change::DebugIndicators);
    }

    if (m_tilesUpdate)
        m_tilesUpdate->merge(WTF::move(*tilesUpdate));
    else
        m_tilesUpdate = WTF::move(*tilesUpdate);
}

void CoordinatedPlatformLayer::updateContents(bool affectedByTransformAnimation)
{
    assertIsMainThread();

    if (needsBackingStore()) {
        if (!m_backingStoreProxy) {
            m_backingStoreProxy = CoordinatedBackingStoreProxy::create();
            m_backingStoreProxy->setAffectedByTransformAnimation(affectedByTransformAnimation);
            m_needsTilesUpdate = true;
            addPendingChange(Change::BackingStore);
        } else {
            bool wasAffectedByTransformAnimation = !!m_backingStoreProxy->animatedBackingStoreClient();
            if (wasAffectedByTransformAnimation != affectedByTransformAnimation) {
                m_backingStoreProxy->setAffectedByTransformAnimation(affectedByTransformAnimation);
                addPendingChange(Change::BackingStore);
            }
        }
    } else {
        if (m_backingStoreProxy) {
            m_backingStoreProxy->invalidate(*this);
            m_backingStoreProxy = nullptr;
            addPendingChange(Change::BackingStore);

            m_tilesUpdate = std::nullopt;
            m_pendingChanges.remove(Change::BackingStoreTiles);
        }
    }
}

void CoordinatedPlatformLayer::purgeBackingStores()
{
    assertIsMainThread();

    if (m_backingStoreProxy) {
        m_backingStoreProxy->invalidate(*this);
        m_backingStoreProxy = nullptr;
    }
    m_tilesUpdate = std::nullopt;
    m_imageBackingStore = nullptr;
    m_contentsBuffer = nullptr;

    {
        Locker locker { m_scrollbarContents.lock };
        m_scrollbarContents.buffer = std::nullopt;
        m_scrollbarContents.rect = std::nullopt;
    }

    {
        Locker locker { m_videoContents.lock };
        if (m_videoContents.buffer.has_value() && shouldReleaseBuffer(m_videoContents.buffer->get()))
            m_videoContents.buffer = std::nullopt;
    }
}

bool CoordinatedPlatformLayer::isCompositionRequiredOrOngoing() const
{
    return m_client ? m_client->isCompositionRequiredOrOngoing() : false;
}

void CoordinatedPlatformLayer::requestComposition(CompositionReason reason)
{
    if (m_client)
        m_client->requestComposition(reason);
}

void CoordinatedPlatformLayer::didApplyLayerScrollingPositions()
{
    if (m_client)
        m_client->didApplyLayerScrollingPositions();
}

RunLoop* CoordinatedPlatformLayer::compositingRunLoop() const
{
    return m_client ? m_client->compositingRunLoop() : nullptr;
}

int CoordinatedPlatformLayer::maxTextureSize() const
{
    return m_client ? m_client->maxTextureSize() : 0;
}

void CoordinatedPlatformLayer::willPaintTile()
{
    ASSERT(isMainThread());
    ASSERT(m_client);
    m_client->willPaintTile();
}

void CoordinatedPlatformLayer::didPaintTile()
{
    // Could be called from painting threads.
    if (m_client)
        m_client->didPaintTile();
}

void CoordinatedPlatformLayer::didPaintPreRenderedTile()
{
    // Called from painting threads.
    if (m_client)
        m_client->didPaintPreRenderedTile(*this);
}

void CoordinatedPlatformLayer::waitUntilPaintingFinished()
{
    assertIsMainThread();

    if (m_backingStoreProxy)
        m_backingStoreProxy->waitUntilPaintingFinished();
}

void CoordinatedPlatformLayer::commitPendingScrollingPosition()
{
    bool needsComposition = false;
    {
        Locker scrollingLocker { m_scrollingPosition.lock };
        if (m_scrollingPosition.pendingPosition) {
            auto pendingPosition = std::exchange(m_scrollingPosition.pendingPosition, std::nullopt);
            if (pendingPosition != m_scrollingPosition.currentPosition) {
                m_scrollingPosition.committedPosition = pendingPosition;
                m_scrollingPosition.currentPosition = pendingPosition;
                needsComposition = true;
            }
        }

        if (m_scrollingPosition.pendingBoundsOrigin) {
            auto pendingBoundsOrigin = std::exchange(m_scrollingPosition.pendingBoundsOrigin, std::nullopt);
            if (pendingBoundsOrigin != m_scrollingPosition.currentBoundsOrigin) {
                m_scrollingPosition.committedBoundsOrigin = pendingBoundsOrigin;
                m_scrollingPosition.currentBoundsOrigin = pendingBoundsOrigin;
                needsComposition = true;
            }
        }
        m_scrollingPosition.didNotifyPositionChange = false;
    }

    if (needsComposition)
        notifyCompositionRequired();
}

void CoordinatedPlatformLayer::flushScrollingPositionChanges(const OptionSet<CompositionReason>& reasons)
{
    ASSERT(!isMainThread());
    if (!reasons.containsAny({ CompositionReason::RenderingUpdate, CompositionReason::AsyncScrolling }))
        return;

    std::optional<FloatPoint> position;
    std::optional<FloatPoint> boundsOrigin;
    {
        Locker scrollingLocker { m_scrollingPosition.lock };
        position = std::exchange(m_scrollingPosition.committedPosition, std::nullopt);
        boundsOrigin = std::exchange(m_scrollingPosition.committedBoundsOrigin, std::nullopt);
    }

    if (!position && !boundsOrigin)
        return;

    auto& layer = ensureTarget();
    if (position)
        layer.setPosition(*position);
    if (boundsOrigin)
        layer.setBoundsOrigin(*boundsOrigin);
}

CoordinatedPlatformLayer::RenderingUpdateProperties::~RenderingUpdateProperties() = default;

bool CoordinatedPlatformLayer::commitPendingRenderingUpdateProperties()
{
    assertIsMainThread();
    if (m_pendingChanges.isEmpty())
        return false;

    RenderingUpdateProperties properties;
    auto changes = std::exchange(m_pendingChanges, { });
    if (changes.containsAny({ Change::Position, Change::BoundsOrigin, Change::AnchorPoint, Change::Size, Change::Transform, Change::MasksToBounds, Change::Mask, Change::Children })) {
        Locker locker { m_lock };
        if (changes.contains(Change::Position))
            properties.position = m_position;
        if (changes.contains(Change::BoundsOrigin))
            properties.boundsOrigin = m_boundsOrigin;
        if (changes.contains(Change::AnchorPoint))
            properties.anchorPoint = m_anchorPoint;
        if (changes.contains(Change::Size))
            properties.size = m_size;
        if (changes.contains(Change::Transform))
            properties.transform = m_transform;
        if (changes.contains(Change::MasksToBounds))
            properties.masksToBounds = m_masksToBounds;
        if (changes.contains(Change::Mask))
            properties.mask = m_mask;
        if (changes.contains(Change::Children))
            properties.children = m_children;
    }

    if (changes.contains(Change::ChildrenTransform))
        properties.childrenTransform = m_childrenTransform;
    if (changes.contains(Change::Preserves3D))
        properties.preserves3D = m_preserves3D;
    if (changes.contains(Change::BackfaceVisibility))
        properties.backfaceVisibility = m_backfaceVisibility;
    if (changes.contains(Change::Opacity))
        properties.opacity = m_opacity;
    if (changes.contains(Change::BlendMode))
        properties.blendMode = m_blendMode;
    if (changes.contains(Change::ContentsVisible))
        properties.contentsVisible = m_contentsVisible;
    if (changes.contains(Change::ContentsOpaque))
        properties.contentsOpaque = m_contentsOpaque;
    if (changes.contains(Change::ContentsRect))
        properties.contentsRect = m_contentsRect;
    if (changes.contains(Change::ContentsRectClipsDescendants))
        properties.contentsRectClipsDescendants = m_contentsRectClipsDescendants;
    if (changes.contains(Change::ContentsClippingRect))
        properties.contentsClippingRect = m_contentsClippingRect;
    if (changes.contains(Change::ContentsClipShapePath))
        properties.contentsClipShapePath = m_contentsClipShapePath;
    if (changes.contains(Change::BackgroundColor))
        properties.backgroundColor = m_backgroundColor;
    if (changes.contains(Change::ContentsColor))
        properties.contentsColor = m_contentsColor;
    if (changes.contains(Change::ContentsTiling)) {
        properties.contentsTileSize = m_contentsTileSize;
        properties.contentsTilePhase = m_contentsTilePhase;
    }
    if (changes.contains(Change::ContentsImage))
        properties.imageBackingStore = m_imageBackingStore;
    if (changes.contains(Change::ContentsBuffer))
        properties.contentsBuffer = WTF::move(m_contentsBuffer);
    if (changes.contains(Change::BackingStore)) {
        properties.hasBackingStore = !!m_backingStoreProxy;
        if (properties.hasBackingStore)
            properties.animatedBackingStoreClient = m_backingStoreProxy->animatedBackingStoreClient();
    }
    if (changes.contains(Change::BackingStoreTiles)) {
        ASSERT(m_tilesUpdate);
        properties.tilesUpdate = std::exchange(m_tilesUpdate, std::nullopt);
        properties.contentsScale = m_contentsScale;
    }
    if (changes.contains(Change::ClipPath)) {
        properties.clipPath = m_clipPath.path;
        properties.clipPathWindRule = m_clipPath.windRule;
    }
    if (changes.contains(Change::Filters))
        properties.filters = m_filters;
    if (changes.contains(Change::Replica))
        properties.replica = m_replica;
    if (changes.contains(Change::Backdrop))
        properties.backdropFilters = m_backdropFilters;
    if (changes.contains(Change::BackdropRect))
        properties.backdropRect = m_backdropRect;
    if (changes.contains(Change::BackdropShapePath))
        properties.backdropShapePath = m_backdropShapePath;
    if (changes.contains(Change::BackdropRoot))
        properties.isBackdropRoot = m_isBackdropRoot;
    if (changes.contains(Change::Animations))
        properties.animations = m_animations;
    if (changes.contains(Change::DebugIndicators)) {
        properties.debugBorderColor = m_debugBorderColor;
        properties.debugBorderWidth = m_debugBorderWidth;
        properties.repaintCount = m_repaintCount;
    }
#if ENABLE(DAMAGE_TRACKING)
    if (changes.contains(Change::Damage))
        properties.damage = std::exchange(m_damage, std::nullopt);
#endif

    properties.changes = WTF::move(changes);

    Locker committedRenderingUpdatesLocker { m_committedRenderingUpdatesLock };
    m_committedRenderingUpdates.append(WTF::move(properties));

    return true;
}

void CoordinatedPlatformLayer::flushCompositingState(const OptionSet<CompositionReason>& reasons)
{
    ASSERT(!isMainThread());
    flushCompositingStateProperties(reasons);
    flushCompositingStateScrollbarContents(reasons);
    flushCompositingStateVideoContents(reasons);
}

void CoordinatedPlatformLayer::flushCompositingStateProperties(const OptionSet<CompositionReason>& reasons)
{
    if (!reasons.contains(CompositionReason::RenderingUpdate))
        return;

    std::optional<RenderingUpdateProperties> properties;
    {
        Locker locker { m_committedRenderingUpdatesLock };

        // FIXME: For now the queue can only have at most one item because
        // we don't allow a layer flush until the previous composition has
        // finished.
        ASSERT(m_committedRenderingUpdates.size() <= 1);
        if (m_committedRenderingUpdates.isEmpty())
            return;

        properties.emplace(m_committedRenderingUpdates.takeFirst());
    }

    auto& layer = ensureTarget();
    const auto& changes = properties->changes;
    if (changes.contains(Change::Position)) {
        Locker locker { m_scrollingPosition.lock };
        layer.setPosition(m_scrollingPosition.currentPosition.value_or(properties->position));
    }
    if (changes.contains(Change::BoundsOrigin)) {
        Locker locker { m_scrollingPosition.lock };
        layer.setBoundsOrigin(m_scrollingPosition.currentBoundsOrigin.value_or(properties->boundsOrigin));
    }
    if (changes.contains(Change::AnchorPoint))
        layer.setAnchorPoint(properties->anchorPoint);
    if (changes.contains(Change::Size))
        layer.setSize(properties->size);
    if (changes.contains(Change::Transform))
        layer.setTransform(properties->transform);
    if (changes.contains(Change::ChildrenTransform))
        layer.setChildrenTransform(properties->childrenTransform);
    if (changes.contains(Change::MasksToBounds))
        layer.setMasksToBounds(properties->masksToBounds);
    if (changes.contains(Change::Preserves3D))
        layer.setPreserves3D(properties->preserves3D);
    if (changes.contains(Change::BackfaceVisibility))
        layer.setBackfaceVisibility(properties->backfaceVisibility);
    if (changes.contains(Change::Opacity))
        layer.setOpacity(properties->opacity);
    if (changes.contains(Change::BlendMode))
        layer.setBlendMode(properties->blendMode);
    if (changes.contains(Change::ContentsVisible))
        layer.setContentsVisible(properties->contentsVisible);
    if (changes.contains(Change::ContentsOpaque))
        layer.setContentsOpaque(properties->contentsOpaque);
    if (changes.contains(Change::ContentsRect))
        layer.setContentsRect(properties->contentsRect);
    if (changes.contains(Change::ContentsRectClipsDescendants))
        layer.setContentsRectClipsDescendants(properties->contentsRectClipsDescendants);
    if (changes.contains(Change::ContentsClippingRect))
        layer.setContentsClippingRect(properties->contentsClippingRect);
    if (changes.contains(Change::ContentsClipShapePath)) {
        if (properties->contentsClipShapePath.isEmpty())
            layer.setContentsClipPath(std::nullopt);
        else {
            auto contentsClipPath = *properties->contentsClipShapePath.platformPath();
            contentsClipPath.setFillType(SkPathFillType::kWinding);
            layer.setContentsClipPath(WTF::move(contentsClipPath));
        }
    }
    if (changes.contains(Change::BackgroundColor))
        layer.setBackgroundColor(WTF::move(properties->backgroundColor));
    if (changes.contains(Change::ContentsColor))
        layer.setContentsSolidColor(WTF::move(properties->contentsColor));
    if (changes.contains(Change::ContentsTiling))
        layer.setContentsTiling(properties->contentsTileSize, properties->contentsTilePhase);
    if (changes.contains(Change::ContentsImage))
        layer.setImageBackingStore(properties->imageBackingStore);
    if (changes.contains(Change::ContentsBuffer))
        layer.setContentsBuffer(WTF::move(properties->contentsBuffer));
    if (changes.contains(Change::BackingStore))
        layer.setUseBackingStore(properties->hasBackingStore, properties->animatedBackingStoreClient);
    if (changes.contains(Change::BackingStoreTiles))
        layer.updateBackingStore(WTF::move(*properties->tilesUpdate), properties->contentsScale);
    if (changes.contains(Change::ClipPath)) {
        auto clipPath = *properties->clipPath.platformPath();
        clipPath.setFillType(properties->clipPathWindRule == WindRule::EvenOdd ? SkPathFillType::kEvenOdd : SkPathFillType::kWinding);
        layer.setClipPath(WTF::move(clipPath));
    }
    if (changes.contains(Change::Filters))
        layer.setFilters(WTF::move(properties->filters));
    if (changes.contains(Change::Mask))
        layer.setMask(properties->mask ? RefPtr { &properties->mask->ensureTarget() } : nullptr);
    if (changes.contains(Change::Replica))
        layer.setReplica(properties->replica ? RefPtr { &properties->replica->ensureTarget() } : nullptr);
    if (changes.contains(Change::Backdrop))
        layer.setBackdropFilters(WTF::move(properties->backdropFilters));
    if (changes.contains(Change::BackdropRect))
        layer.setBackdropFiltersRect(properties->backdropRect);
    if (changes.contains(Change::BackdropShapePath)) {
        if (properties->backdropShapePath.isEmpty())
            layer.setBackdropFiltersClipPath(std::nullopt);
        else {
            auto backdropClipPath = *properties->backdropShapePath.platformPath();
            backdropClipPath.setFillType(SkPathFillType::kWinding);
            layer.setBackdropFiltersClipPath(WTF::move(backdropClipPath));
        }
    }
    if (changes.contains(Change::BackdropRoot))
        layer.setIsBackdropRoot(properties->isBackdropRoot);
    if (changes.contains(Change::Animations))
        layer.setAnimations(WTF::move(properties->animations));
    if (changes.contains(Change::Children)) {
        layer.setChildren(WTF::map(properties->children, [](auto& child) {
            return Ref { child->ensureTarget() };
        }));
    }
    if (changes.contains(Change::DebugIndicators)) {
        Color color;
        std::optional<float> width;
        if (properties->debugBorderColor.isVisible()) {
            color = properties->debugBorderColor;
            width = properties->debugBorderWidth;
        }
        std::optional<unsigned> repaintCount;
        if (properties->repaintCount != -1)
            repaintCount = properties->repaintCount;
        layer.setDebugIndicators(WTF::move(color), width, repaintCount);
    }
#if ENABLE(DAMAGE_TRACKING)
    if (changes.contains(Change::Damage)) {
        ASSERT(properties->damage.has_value());
        layer.addDamage(*std::exchange(properties->damage, std::nullopt));
    }
#endif
}

void CoordinatedPlatformLayer::flushCompositingStateScrollbarContents(const OptionSet<CompositionReason>& reasons)
{
    Locker locker { m_scrollbarContents.lock };
    if (!m_scrollbarContents.buffer.has_value() || !reasons.containsAny({ CompositionReason::RenderingUpdate, CompositionReason::AsyncScrolling }))
        return;

    auto& layer = ensureTarget();

    if (auto rect = std::exchange(m_scrollbarContents.rect, std::nullopt)) {
        layer.setContentsRect(*rect);
        layer.setContentsClippingRect(FloatRoundedRect(*rect));
    }

    auto buffer = std::exchange(m_scrollbarContents.buffer, std::nullopt);
    if (buffer.has_value() && (buffer.value() || layer.contentsBuffer())) {
        layer.setContentsBuffer(WTF::move(*buffer));
        layer.damageWholeLayer();
    }
}

void CoordinatedPlatformLayer::flushCompositingStateVideoContents(const OptionSet<CompositionReason>& reasons)
{
    Locker locker { m_videoContents.lock };
    if (!m_videoContents.buffer.has_value() || !reasons.containsAny({ CompositionReason::RenderingUpdate, CompositionReason::VideoFrame }))
        return;

    auto& layer = ensureTarget();

    auto buffer = std::exchange(m_videoContents.buffer, std::nullopt);
    if (buffer.has_value() && (buffer.value() || layer.contentsBuffer())) {
        layer.setContentsBuffer(WTF::move(*buffer));
        layer.damageWholeLayer();
    }
}

bool CoordinatedPlatformLayer::hasPendingBackingStoreTileUpdates() const
{
    ASSERT(!isMainThread());
    return m_target ? m_target->hasPendingBackingStoreTileUpdates() : false;
}

void CoordinatedPlatformLayer::processPendingBackingStoreTileUpdates()
{
    ASSERT(!isMainThread());
    if (m_target)
        m_target->processPendingTileUpdates();
}

bool CoordinatedPlatformLayer::processPendingBackingStoreTileUpdatesIncludingPreRendered(const Function<bool()>& shouldStopProcessingTiles)
{
    ASSERT(!isMainThread());
    return m_target ? m_target->processPendingTileUpdatesIncludingPreRendered(shouldStopProcessingTiles) : false;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && !USE(TEXTURE_MAPPER)
