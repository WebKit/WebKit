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

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedAnimatedBackingStoreClient.h"
#include "CoordinatedBackingStore.h"
#include "CoordinatedBackingStoreProxy.h"
#include "CoordinatedImageBackingStore.h"
#include "CoordinatedPlatformLayerBuffer.h"
#include "CoordinatedPlatformLayerBufferHolePunch.h"
#include "CoordinatedPlatformLayerBufferVideo.h"
#include "CoordinatedTileBuffer.h"
#include "GraphicsContext.h"
#include "GraphicsLayerCoordinated.h"
#include "NativeImage.h"
#include "TextureMapperLayer.h"
#include <wtf/MainThread.h>

#if USE(CAIRO)
#include "CairoPaintingContext.h"
#include "CairoPaintingEngine.h"
#endif

#if USE(SKIA)
#include "SkiaPaintingEngine.h"
#include "SkiaRecordingResult.h"
#endif

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
{
    ASSERT(isMainThread());
}

CoordinatedPlatformLayer::~CoordinatedPlatformLayer() = default;

void CoordinatedPlatformLayer::setOwner(GraphicsLayerCoordinated* owner)
{
    ASSERT(isMainThread());
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
    ASSERT(isMainThread());
    return m_owner;
}

TextureMapperLayer& CoordinatedPlatformLayer::ensureTarget()
{
    ASSERT(!isMainThread());
    if (!m_target) {
        m_target = makeUnique<TextureMapperLayer>();
#if ENABLE(DAMAGE_TRACKING)
        m_target->setDamagePropagationEnabled(m_damagePropagationEnabled);
        if (m_damagePropagationEnabled)
            m_target->setDamageInGlobalCoordinateSpace(m_damageInGlobalCoordinateSpace);
#endif
    }
    return *m_target;
}

TextureMapperLayer* CoordinatedPlatformLayer::target() const
{
    ASSERT(!isMainThread());
    return m_target.get();
}

static bool shouldReleaseBuffer(CoordinatedPlatformLayerBuffer* buffer)
{
    if (!buffer)
        return false;

#if ENABLE(VIDEO)
    // Do not release hole punch buffers early. See https://bugs.webkit.org/show_bug.cgi?id=267322.
    if (is<CoordinatedPlatformLayerBufferHolePunch>(*buffer))
        return false;
#endif

    return true;
}

void CoordinatedPlatformLayer::invalidateTarget()
{
    ASSERT(!isMainThread());
    {
        Locker locker { m_lock };
        m_backingStore = nullptr;
        m_imageBackingStore.committed = nullptr;
        if (shouldReleaseBuffer(m_contentsBuffer.committed.get()))
            m_contentsBuffer.committed = nullptr;
    }
    m_target = nullptr;
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

void CoordinatedPlatformLayer::setPosition(FloatPoint&& position)
{
    ASSERT(m_lock.isHeld());
    if (m_position == position)
        return;

    m_position = WTFMove(position);
    m_pendingChanges.add(Change::Position);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setPositionForScrolling(const FloatPoint& position, ForcePositionSync forceSync)
{
    Locker locker { m_lock };
    if (m_position == position && forceSync == ForcePositionSync::No)
        return;

    m_position = position;
    m_pendingChanges.add(Change::Position);
    notifyCompositionRequired();
}

const FloatPoint& CoordinatedPlatformLayer::position() const
{
    ASSERT(m_lock.isHeld());
    return m_position;
}

void CoordinatedPlatformLayer::setTopLeftPositionForScrolling(const FloatPoint& position, ForcePositionSync forceSync)
{
    FloatPoint newPosition;
    {
        Locker locker { m_lock };
        newPosition = { position.x() + m_anchorPoint.x() * m_size.width(), position.y() + m_anchorPoint.y() * m_size.height() };
    }
    setPositionForScrolling(newPosition, forceSync);
}

FloatPoint CoordinatedPlatformLayer::topLeftPositionForScrolling()
{
    Locker locker { m_lock };
    return m_position - toFloatSize(m_anchorPoint.xy()) * m_size;
}

void CoordinatedPlatformLayer::setBoundsOrigin(const FloatPoint& origin)
{
    ASSERT(m_lock.isHeld());
    if (m_boundsOrigin == origin)
        return;

    m_boundsOrigin = origin;
    m_pendingChanges.add(Change::BoundsOrigin);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setBoundsOriginForScrolling(const FloatPoint& origin)
{
    Locker locker { m_lock };
    if (m_boundsOrigin == origin)
        return;

    m_boundsOrigin = origin;
    m_pendingChanges.add(Change::BoundsOrigin);
    notifyCompositionRequired();
}

const FloatPoint& CoordinatedPlatformLayer::boundsOrigin() const
{
    ASSERT(m_lock.isHeld());
    return m_boundsOrigin;
}

void CoordinatedPlatformLayer::setAnchorPoint(FloatPoint3D&& point)
{
    ASSERT(m_lock.isHeld());
    if (m_anchorPoint == point)
        return;

    m_anchorPoint = WTFMove(point);
    m_pendingChanges.add(Change::AnchorPoint);
    notifyCompositionRequired();
}

const FloatPoint3D& CoordinatedPlatformLayer::anchorPoint() const
{
    ASSERT(m_lock.isHeld());
    return m_anchorPoint;
}

void CoordinatedPlatformLayer::setSize(FloatSize&& size)
{
    ASSERT(m_lock.isHeld());
    if (m_size == size)
        return;

    m_size = WTFMove(size);
    m_pendingChanges.add(Change::Size);
    notifyCompositionRequired();
}

const FloatSize& CoordinatedPlatformLayer::size() const
{
    ASSERT(m_lock.isHeld());
    return m_size;
}

FloatRect CoordinatedPlatformLayer::bounds() const
{
    ASSERT(m_lock.isHeld());
    return FloatRect({ }, m_size);
}

void CoordinatedPlatformLayer::setTransform(const TransformationMatrix& matrix)
{
    ASSERT(m_lock.isHeld());
    if (m_transform == matrix)
        return;

    m_transform = matrix;
    m_pendingChanges.add(Change::Transform);
    notifyCompositionRequired();
}

const TransformationMatrix& CoordinatedPlatformLayer::transform() const
{
    ASSERT(m_lock.isHeld());
    return m_transform;
}

void CoordinatedPlatformLayer::setChildrenTransform(const TransformationMatrix& matrix)
{
    ASSERT(m_lock.isHeld());
    if (m_childrenTransform == matrix)
        return;

    m_childrenTransform = matrix;
    m_pendingChanges.add(Change::ChildrenTransform);
    notifyCompositionRequired();
}

const TransformationMatrix& CoordinatedPlatformLayer::childrenTransform() const
{
    ASSERT(m_lock.isHeld());
    return m_childrenTransform;
}

void CoordinatedPlatformLayer::didUpdateLayerTransform()
{
    m_needsTilesUpdate = true;
}

void CoordinatedPlatformLayer::setVisibleRect(const FloatRect& visibleRect)
{
    ASSERT(m_lock.isHeld());
    if (m_visibleRect == visibleRect)
        return;

    m_visibleRect = visibleRect;
}

const FloatRect& CoordinatedPlatformLayer::visibleRect() const
{
    ASSERT(m_lock.isHeld());
    return m_visibleRect;
}

void CoordinatedPlatformLayer::setTransformedVisibleRect(IntRect&& transformedVisibleRect, IntRect&& transformedVisibleRectIncludingFuture)
{
    ASSERT(m_lock.isHeld());
    if (m_transformedVisibleRect == transformedVisibleRect && m_transformedVisibleRectIncludingFuture == transformedVisibleRectIncludingFuture)
        return;

    m_transformedVisibleRect = WTFMove(transformedVisibleRect);
    m_transformedVisibleRectIncludingFuture = WTFMove(transformedVisibleRectIncludingFuture);
    m_needsTilesUpdate = true;
}

#if ENABLE(SCROLLING_THREAD)
void CoordinatedPlatformLayer::setScrollingNodeID(std::optional<ScrollingNodeID> nodeID)
{
    ASSERT(m_lock.isHeld());
    if (m_scrollingNodeID == nodeID)
        return;

    m_scrollingNodeID = nodeID;
    m_pendingChanges.add(Change::ScrollingNode);
    notifyCompositionRequired();
}

const Markable<ScrollingNodeID>& CoordinatedPlatformLayer::scrollingNodeID() const
{
    ASSERT(m_lock.isHeld());
    return m_scrollingNodeID;
}
#endif

void CoordinatedPlatformLayer::setDrawsContent(bool drawsContent)
{
    ASSERT(m_lock.isHeld());
    if (m_drawsContent == drawsContent)
        return;

    m_drawsContent = drawsContent;
    m_pendingChanges.add(Change::DrawsContent);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setMasksToBounds(bool masksToBounds)
{
    ASSERT(m_lock.isHeld());
    if (m_masksToBounds == masksToBounds)
        return;

    m_masksToBounds = masksToBounds;
    m_pendingChanges.add(Change::MasksToBounds);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setPreserves3D(bool preserves3D)
{
    ASSERT(m_lock.isHeld());
    if (m_preserves3D == preserves3D)
        return;

    m_preserves3D = preserves3D;
    m_pendingChanges.add(Change::Preserves3D);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setBackfaceVisibility(bool backfaceVisibility)
{
    ASSERT(m_lock.isHeld());
    if (m_backfaceVisibility == backfaceVisibility)
        return;

    m_backfaceVisibility = backfaceVisibility;
    m_pendingChanges.add(Change::BackfaceVisibility);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setOpacity(float opacity)
{
    ASSERT(m_lock.isHeld());
    if (m_opacity == opacity)
        return;

    m_opacity = opacity;
    m_pendingChanges.add(Change::Opacity);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsVisible(bool contentsVisible)
{
    ASSERT(m_lock.isHeld());
    if (m_contentsVisible == contentsVisible)
        return;

    m_contentsVisible = contentsVisible;
    m_pendingChanges.add(Change::ContentsVisible);
    notifyCompositionRequired();
}

bool CoordinatedPlatformLayer::contentsVisible() const
{
    ASSERT(m_lock.isHeld());
    return m_contentsVisible;
}

void CoordinatedPlatformLayer::setContentsOpaque(bool contentsOpaque)
{
    ASSERT(m_lock.isHeld());
    if (m_contentsOpaque == contentsOpaque)
        return;

    m_contentsOpaque = contentsOpaque;
    m_pendingChanges.add(Change::ContentsOpaque);
    // FIXME: request a full repaint?
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsRect(const FloatRect& contentsRect)
{
    ASSERT(m_lock.isHeld());
    if (m_contentsRect == contentsRect)
        return;

    m_contentsRect = contentsRect;
    m_pendingChanges.add(Change::ContentsRect);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsRectClipsDescendants(bool contentsRectClipsDescendants)
{
    ASSERT(m_lock.isHeld());
    if (m_contentsRectClipsDescendants == contentsRectClipsDescendants)
        return;

    m_contentsRectClipsDescendants = contentsRectClipsDescendants;
    m_pendingChanges.add(Change::ContentsRectClipsDescendants);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsClippingRect(const FloatRoundedRect& contentsClippingRect)
{
    ASSERT(m_lock.isHeld());
    if (m_contentsClippingRect == contentsClippingRect)
        return;

    m_contentsClippingRect = contentsClippingRect;
    m_pendingChanges.add(Change::ContentsClippingRect);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsScale(float contentsScale)
{
    ASSERT(m_lock.isHeld());
    if (m_contentsScale == contentsScale)
        return;

    m_contentsScale = contentsScale;
    m_needsTilesUpdate = true;
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&& buffer, std::optional<Damage>&& dirtyRegion, RequireComposition requireComposition)
{
    ASSERT(m_lock.isHeld());
    if (!buffer && !m_contentsBuffer.pending && !m_contentsBuffer.committed)
        return;

    m_contentsBuffer.pending = WTFMove(buffer);
    m_pendingChanges.add(Change::ContentsBuffer);
#if ENABLE(DAMAGE_TRACKING)
    if (dirtyRegion)
        addDamage(WTFMove(*dirtyRegion));
#else
    UNUSED_PARAM(dirtyRegion);
#endif
    if (requireComposition == RequireComposition::Yes)
        notifyCompositionRequired();
}

#if ENABLE(VIDEO) && USE(GSTREAMER)
void CoordinatedPlatformLayer::replaceCurrentContentsBufferWithCopy()
{
    Locker locker { m_lock };
    if (!m_contentsBuffer.committed)
        return;

    m_contentsBuffer.pending = nullptr;
    if (is<CoordinatedPlatformLayerBufferVideo>(*m_contentsBuffer.committed))
        m_contentsBuffer.pending = downcast<CoordinatedPlatformLayerBufferVideo>(*m_contentsBuffer.committed).copyBuffer();
    m_contentsBuffer.committed = WTFMove(m_contentsBuffer.pending);
    ensureTarget().setContentsLayer(m_contentsBuffer.committed.get());
}
#endif

void CoordinatedPlatformLayer::setContentsImage(NativeImage* image)
{
    ASSERT(m_lock.isHeld());
    if (image) {
        if (m_imageBackingStore.current && m_imageBackingStore.current->isSameNativeImage(*image))
            return;

        ASSERT(m_client);
        m_imageBackingStore.current = m_client->imageBackingStore(Ref { *image });
    } else {
        if (!m_imageBackingStore.current)
            return;
        m_imageBackingStore.current = nullptr;
    }
    m_pendingChanges.add(Change::ContentsImage);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsColor(const Color& color)
{
    ASSERT(m_lock.isHeld());
    if (m_contentsColor == color)
        return;

    m_contentsColor = color;
    m_pendingChanges.add(Change::ContentsColor);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsTileSize(const FloatSize& contentsTileSize)
{
    ASSERT(m_lock.isHeld());
    if (m_contentsTileSize == contentsTileSize)
        return;

    m_contentsTileSize = contentsTileSize;
    m_pendingChanges.add(Change::ContentsTiling);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsTilePhase(const FloatSize& contentsTilePhase)
{
    ASSERT(m_lock.isHeld());
    if (m_contentsTilePhase == contentsTilePhase)
        return;

    m_contentsTilePhase = contentsTilePhase;
    m_pendingChanges.add(Change::ContentsTiling);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setDirtyRegion(Damage&& damage)
{
    ASSERT(m_lock.isHeld());
    auto dirtyRegion = damage.rects();
    if (m_dirtyRegion != dirtyRegion) {
        m_dirtyRegion = WTFMove(dirtyRegion);
        notifyCompositionRequired();
    }

#if ENABLE(DAMAGE_TRACKING)
    addDamage(WTFMove(damage));
#endif
}

#if ENABLE(DAMAGE_TRACKING)
void CoordinatedPlatformLayer::addDamage(Damage&& damage)
{
    if (!m_damage)
        m_damage = WTFMove(damage);
    else
        m_damage->add(damage);
    m_pendingChanges.add(Change::Damage);
}
#endif

void CoordinatedPlatformLayer::setFilters(const FilterOperations& filters)
{
    ASSERT(m_lock.isHeld());
    if (m_filters == filters)
        return;

    m_filters = filters;
    m_pendingChanges.add(Change::Filters);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setMask(CoordinatedPlatformLayer* mask)
{
    ASSERT(m_lock.isHeld());
    if (m_mask == mask)
        return;

    m_mask = mask;
    m_pendingChanges.add(Change::Mask);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setReplica(CoordinatedPlatformLayer* replica)
{
    ASSERT(m_lock.isHeld());
    if (m_replica == replica)
        return;

    m_replica = replica;
    m_pendingChanges.add(Change::Replica);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setBackdrop(CoordinatedPlatformLayer* backdrop)
{
    ASSERT(m_lock.isHeld());
    if (m_backdrop == backdrop)
        return;

    m_backdrop = backdrop;
    m_pendingChanges.add(Change::Backdrop);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setBackdropRect(const FloatRoundedRect& backdropRect)
{
    ASSERT(m_lock.isHeld());
    if (m_backdropRect == backdropRect)
        return;

    m_backdropRect = backdropRect;
    m_pendingChanges.add(Change::BackdropRect);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setAnimations(const TextureMapperAnimations& animations)
{
    ASSERT(m_lock.isHeld());
    m_animations = animations;
    m_pendingChanges.add(Change::Animations);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setChildren(Vector<Ref<CoordinatedPlatformLayer>>&& children)
{
    ASSERT(m_lock.isHeld());
    if (m_children == children)
        return;

    m_children = WTFMove(children);
    m_pendingChanges.add(Change::Children);
    notifyCompositionRequired();
}

const Vector<Ref<CoordinatedPlatformLayer>>& CoordinatedPlatformLayer::children() const
{
    ASSERT(m_lock.isHeld());
    return m_children;
}

void CoordinatedPlatformLayer::setEventRegion(const EventRegion& eventRegion)
{
    ASSERT(m_lock.isHeld());
    m_eventRegion = eventRegion;
}

const EventRegion& CoordinatedPlatformLayer::eventRegion() const
{
    ASSERT(m_lock.isHeld());
    return m_eventRegion;
}

void CoordinatedPlatformLayer::setDebugBorder(Color&& borderColor, float borderWidth)
{
    ASSERT(m_lock.isHeld());
    if (m_debugBorderColor == borderColor && m_debugBorderWidth == borderWidth)
        return;

    m_debugBorderColor = WTFMove(borderColor);
    m_debugBorderWidth = borderWidth;
    m_pendingChanges.add(Change::DebugIndicators);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setShowRepaintCounter(bool showRepaintCounter)
{
    ASSERT(m_lock.isHeld());
    if ((m_repaintCount != -1 && showRepaintCounter) || (m_repaintCount == -1 && !showRepaintCounter))
        return;

    m_repaintCount = showRepaintCounter ? m_owner->repaintCount() : -1;
    m_pendingChanges.add(Change::DebugIndicators);
    notifyCompositionRequired();
}

bool CoordinatedPlatformLayer::needsBackingStore() const
{
    ASSERT(m_lock.isHeld());
    if (!m_owner)
        return false;

    if (!m_drawsContent || !m_contentsVisible || m_size.isEmpty())
        return false;

    // If the CSS opacity value is 0 and there's no animation over the opacity property, the layer is invisible.
    if (!m_opacity && !m_animations.hasActiveAnimationsOfType(AnimatedProperty::Opacity))
        return false;

    // Check if there's a filter that sets the opacity to zero.
    bool hasOpacityZeroFilter = std::ranges::any_of(m_filters, [](auto& operation) {
        return operation->type() == FilterOperation::Type::Opacity && !downcast<BasicComponentTransferFilterOperation>(operation.get()).amount();
    });

    return !hasOpacityZeroFilter;
}

void CoordinatedPlatformLayer::updateBackingStore()
{
    Locker locker { m_lock };
    if (!m_backingStoreProxy)
        return;

    if (m_dirtyRegion.isEmpty() && !m_pendingTilesCreation && !m_needsTilesUpdate)
        return;

    IntRect contentsRect(IntPoint::zero(), IntSize(m_size));
    auto updateResult = m_backingStoreProxy->updateIfNeeded(m_transformedVisibleRectIncludingFuture, contentsRect, m_contentsScale, m_pendingTilesCreation || m_needsTilesUpdate, m_dirtyRegion, *this);
    m_needsTilesUpdate = false;
    m_dirtyRegion.clear();
    if (m_animatedBackingStoreClient)
        m_animatedBackingStoreClient->update(m_visibleRect, m_backingStoreProxy->coverRect(), m_size, m_contentsScale);

    if (updateResult.contains(CoordinatedBackingStoreProxy::UpdateResult::TilesChanged)) {
        if (m_repaintCount != -1 && updateResult.contains(CoordinatedBackingStoreProxy::UpdateResult::BuffersChanged)) {
            m_repaintCount = m_owner->incrementRepaintCount();
            m_pendingChanges.add(Change::DebugIndicators);
        }
        notifyCompositionRequired();
    }

    m_pendingTilesCreation = updateResult.contains(CoordinatedBackingStoreProxy::UpdateResult::TilesPending);
}

void CoordinatedPlatformLayer::updateContents(bool affectedByTransformAnimation)
{
    ASSERT(m_lock.isHeld());

    if (needsBackingStore()) {
        if (!m_backingStoreProxy) {
            m_backingStoreProxy = CoordinatedBackingStoreProxy::create();
            m_needsTilesUpdate = true;
            m_pendingChanges.add(Change::BackingStore);
        }

        if (affectedByTransformAnimation) {
            if (!m_animatedBackingStoreClient) {
                m_animatedBackingStoreClient = CoordinatedAnimatedBackingStoreClient::create(*m_owner);
                m_pendingChanges.add(Change::BackingStore);
            }
        } else if (m_animatedBackingStoreClient) {
            m_animatedBackingStoreClient->invalidate();
            m_animatedBackingStoreClient = nullptr;
            m_pendingChanges.add(Change::BackingStore);
        }
    } else {
        if (m_backingStoreProxy) {
            m_backingStoreProxy = nullptr;
            m_pendingChanges.add(Change::BackingStore);
        }
        if (m_animatedBackingStoreClient) {
            m_animatedBackingStoreClient->invalidate();
            m_animatedBackingStoreClient = nullptr;
            m_pendingChanges.add(Change::BackingStore);
        }
    }

    if (m_backdrop) {
        Locker locker { m_backdrop->lock() };
        m_backdrop->updateContents(affectedByTransformAnimation);
    }
}

void CoordinatedPlatformLayer::purgeBackingStores()
{
    Locker locker { m_lock };
    m_backingStoreProxy = nullptr;
    if (m_animatedBackingStoreClient) {
        m_animatedBackingStoreClient->invalidate();
        m_animatedBackingStoreClient = nullptr;
    }
    m_imageBackingStore.current = nullptr;
    if (shouldReleaseBuffer(m_contentsBuffer.pending.get()))
        m_contentsBuffer.pending = nullptr;
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

Ref<CoordinatedTileBuffer> CoordinatedPlatformLayer::paint(const IntRect& dirtyRect)
{
    ASSERT(m_lock.isHeld());
    ASSERT(m_client);
    ASSERT(m_owner);
#if USE(CAIRO)
    FloatRect scaledDirtyRect(dirtyRect);
    scaledDirtyRect.scale(1 / m_contentsScale);

    auto buffer = CoordinatedUnacceleratedTileBuffer::create(dirtyRect.size(), m_contentsOpaque ? CoordinatedTileBuffer::NoFlags : CoordinatedTileBuffer::SupportsAlpha);
    m_client->paintingEngine().paint(*m_owner, buffer.get(), dirtyRect, enclosingIntRect(scaledDirtyRect), IntRect { { }, dirtyRect.size() }, m_contentsScale);
    return buffer;
#elif USE(SKIA)
    auto& paintingEngine = m_client->paintingEngine();
    ASSERT(!paintingEngine.useThreadedRendering());
    return paintingEngine.paint(*m_owner, dirtyRect, m_contentsOpaque, m_contentsScale);
#endif
}

#if USE(SKIA)
Ref<SkiaRecordingResult> CoordinatedPlatformLayer::record(const IntRect& recordRect)
{
    ASSERT(m_lock.isHeld());
    ASSERT(m_client);
    ASSERT(m_owner);
    auto& paintingEngine = m_client->paintingEngine();
    ASSERT(paintingEngine.useThreadedRendering());
    return paintingEngine.record(*m_owner, recordRect, m_contentsOpaque, m_contentsScale);
}

Ref<CoordinatedTileBuffer> CoordinatedPlatformLayer::replay(const RefPtr<SkiaRecordingResult>& recording, const IntRect& dirtyRect)
{
    ASSERT(m_lock.isHeld());
    ASSERT(m_client);
    ASSERT(m_owner);
    ASSERT(recording);
    auto& paintingEngine = m_client->paintingEngine();
    ASSERT(paintingEngine.useThreadedRendering());
    return paintingEngine.replay(*m_owner, recording, dirtyRect);
}
#endif

void CoordinatedPlatformLayer::waitUntilPaintingComplete()
{
    Locker locker { m_lock };
    if (m_backingStoreProxy)
        m_backingStoreProxy->waitUntilPaintingComplete();
}

void CoordinatedPlatformLayer::flushCompositingState(const OptionSet<CompositionReason>& reasons, TextureMapper& textureMapper)
{
    ASSERT(!isMainThread());
    Locker locker { m_lock };
    if (m_pendingChanges.isEmpty() && (!reasons.contains(CompositionReason::RenderingUpdate) || !m_backingStoreProxy))
        return;

    auto& layer = ensureTarget();
    if (reasons.containsAny({ CompositionReason::RenderingUpdate, CompositionReason::AsyncScrolling })) {
        if (m_pendingChanges.contains(Change::Position)) {
            layer.setPosition(m_position);
            m_pendingChanges.remove(Change::Position);
        }

        if (m_pendingChanges.contains(Change::BoundsOrigin)) {
            layer.setBoundsOrigin(m_boundsOrigin);
            m_pendingChanges.remove(Change::BoundsOrigin);
        }
    }

    if (reasons.contains(CompositionReason::RenderingUpdate)) {
        if (m_pendingChanges.contains(Change::AnchorPoint)) {
            layer.setAnchorPoint(m_anchorPoint);
            m_pendingChanges.remove(Change::AnchorPoint);
        }

        if (m_pendingChanges.contains(Change::Size)) {
            layer.setSize(m_size);
            m_pendingChanges.remove(Change::Size);
        }

        if (m_pendingChanges.contains(Change::Transform)) {
            layer.setTransform(m_transform);
            m_pendingChanges.remove(Change::Transform);
        }

        if (m_pendingChanges.contains(Change::ChildrenTransform)) {
            layer.setChildrenTransform(m_childrenTransform);
            m_pendingChanges.remove(Change::ChildrenTransform);
        }

        if (m_pendingChanges.contains(Change::Preserves3D)) {
            layer.setPreserves3D(m_preserves3D);
            m_pendingChanges.remove(Change::Preserves3D);
        }

        if (m_pendingChanges.contains(Change::MasksToBounds)) {
            layer.setMasksToBounds(m_masksToBounds);
            m_pendingChanges.remove(Change::MasksToBounds);
        }

        if (m_pendingChanges.contains(Change::BackfaceVisibility)) {
            layer.setBackfaceVisibility(m_backfaceVisibility);
            m_pendingChanges.remove(Change::BackfaceVisibility);
        }

        if (m_pendingChanges.contains(Change::Opacity)) {
            layer.setOpacity(m_opacity);
            m_pendingChanges.remove(Change::Opacity);
        }

        if (m_pendingChanges.contains(Change::BackingStore)) {
            if (m_backingStoreProxy) {
                if (!m_backingStore)
                    m_backingStore = CoordinatedBackingStore::create();
                layer.setBackingStore(m_backingStore.get());

                if (m_animatedBackingStoreClient)
                    layer.setAnimatedBackingStoreClient(m_animatedBackingStoreClient.get());
            } else {
                layer.setBackingStore(nullptr);
                layer.setAnimatedBackingStoreClient(nullptr);
                m_backingStore = nullptr;
            }
            m_pendingChanges.remove(Change::BackingStore);
        }

        if (m_pendingChanges.contains(Change::ContentsVisible)) {
            layer.setContentsVisible(m_contentsVisible);
            m_pendingChanges.remove(Change::ContentsVisible);
        }

        if (m_pendingChanges.contains(Change::ContentsOpaque)) {
            layer.setContentsOpaque(m_contentsOpaque);
            m_pendingChanges.remove(Change::ContentsOpaque);
        }

        if (m_pendingChanges.contains(Change::ContentsRect)) {
            layer.setContentsRect(m_contentsRect);
            m_pendingChanges.remove(Change::ContentsRect);
        }

        if (m_pendingChanges.contains(Change::ContentsRectClipsDescendants)) {
            layer.setContentsRectClipsDescendants(m_contentsRectClipsDescendants);
            m_pendingChanges.remove(Change::ContentsRectClipsDescendants);
        }

        if (m_pendingChanges.contains(Change::ContentsTiling)) {
            layer.setContentsTileSize(m_contentsTileSize);
            layer.setContentsTilePhase(m_contentsTilePhase);
            m_pendingChanges.remove(Change::ContentsTiling);
        }

        if (m_pendingChanges.contains(Change::ContentsClippingRect)) {
            layer.setContentsClippingRect(m_contentsClippingRect);
            m_pendingChanges.remove(Change::ContentsClippingRect);
        }

        if (m_pendingChanges.contains(Change::ContentsImage)) {
            m_imageBackingStore.committed = m_imageBackingStore.current;
            m_pendingChanges.remove(Change::ContentsImage);
        }

        if (m_pendingChanges.contains(Change::ContentsColor)) {
            layer.setSolidColor(m_contentsColor);
            m_pendingChanges.remove(Change::ContentsColor);
        }

#if ENABLE(DAMAGE_TRACKING)
        if (m_pendingChanges.contains(Change::Damage)) {
            ASSERT(m_damage.has_value());
            layer.setDamage(*std::exchange(m_damage, std::nullopt));
            m_pendingChanges.remove(Change::Damage);
        }
#endif

        if (m_pendingChanges.contains(Change::Filters)) {
            layer.setFilters(m_filters);
            m_pendingChanges.remove(Change::Filters);
        }

        if (m_pendingChanges.contains(Change::Mask)) {
            layer.setMaskLayer(m_mask ? &m_mask->ensureTarget() : nullptr);
            m_pendingChanges.remove(Change::Mask);
        }

        if (m_pendingChanges.contains(Change::Replica)) {
            layer.setReplicaLayer(m_replica ? &m_replica->ensureTarget() : nullptr);
            m_pendingChanges.remove(Change::Replica);
        }

        if (m_pendingChanges.contains(Change::Backdrop)) {
            layer.setBackdropLayer(m_backdrop ? &m_backdrop->ensureTarget() : nullptr);
            m_pendingChanges.remove(Change::Backdrop);
        }

        if (m_pendingChanges.contains(Change::BackdropRect)) {
            layer.setBackdropFiltersRect(m_backdropRect);
            m_pendingChanges.remove(Change::BackdropRect);
        }

        if (m_pendingChanges.contains(Change::Animations)) {
            layer.setAnimations(m_animations);
            m_pendingChanges.remove(Change::Animations);
        }

        if (m_pendingChanges.contains(Change::DebugIndicators)) {
            layer.setShowRepaintCounter(m_repaintCount != -1);
            layer.setRepaintCount(m_repaintCount);

            layer.setShowDebugBorder(m_debugBorderColor.isVisible());
            layer.setDebugBorderColor(m_debugBorderColor);
            layer.setDebugBorderWidth(m_debugBorderWidth);
            m_pendingChanges.remove(Change::DebugIndicators);
        }

        if (m_pendingChanges.contains(Change::Children)) {
            layer.setChildren(WTF::map(m_children, [](auto& child) {
                return &child->ensureTarget();
            }));
            m_pendingChanges.remove(Change::Children);
        }

        if (m_backingStoreProxy) {
            m_backingStore->resize(layer.size(), m_contentsScale);

            auto update = m_backingStoreProxy->takePendingUpdate();
            for (auto tileID : update.tilesToCreate())
                m_backingStore->createTile(tileID);
            for (auto tileID : update.tilesToRemove())
                m_backingStore->removeTile(tileID);
            for (const auto& tileUpdate : update.tilesToUpdate())
                m_backingStore->updateTile(tileUpdate.tileID, tileUpdate.dirtyRect, tileUpdate.tileRect, tileUpdate.buffer.copyRef(), { });

            m_backingStore->processPendingUpdates(textureMapper);
        }
    }

    if (reasons.containsAny({ CompositionReason::RenderingUpdate, CompositionReason::VideoFrame })) {
        if (m_pendingChanges.contains(Change::ContentsBuffer)) {
            m_contentsBuffer.committed = WTFMove(m_contentsBuffer.pending);
            m_pendingChanges.remove(Change::ContentsBuffer);
        }

        if (m_contentsBuffer.committed)
            layer.setContentsLayer(m_contentsBuffer.committed.get());
        else if (m_imageBackingStore.committed) {
            if (reasons.contains(CompositionReason::RenderingUpdate))
                layer.setContentsLayer(m_imageBackingStore.committed->buffer());
        } else
            layer.setContentsLayer(nullptr);
    }
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
