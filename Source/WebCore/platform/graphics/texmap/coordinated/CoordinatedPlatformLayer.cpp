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

#if USE(SKIA)
#include "SkiaCompositingLayer.h"
#include "SkiaPaintingEngine.h"
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

#if USE(SKIA)
SkiaCompositingLayer& CoordinatedPlatformLayer::ensureSkiaTarget()
{
    ASSERT(!isMainThread());
    if (!m_skiaTarget)
        m_skiaTarget = SkiaCompositingLayer::create();
#if ENABLE(DAMAGE_TRACKING)
    m_skiaTarget->setDamagePropagationEnabled(m_damagePropagationEnabled);
#endif
    return *m_skiaTarget;
}
#endif

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
    {
        Locker locker { m_lock };
        m_backingStore = nullptr;
        m_imageBackingStore.committed = nullptr;
        if (m_target && shouldReleaseBuffer(m_contentsBuffer.committed.get()))
            m_contentsBuffer.committed = nullptr;
#if USE(SKIA)
        if (m_skiaTarget && !shouldReleaseBuffer(m_skiaTarget->contentsBuffer()))
            m_contentsBuffer.committed = m_skiaTarget->takeContentsBuffer();
#endif
        m_contentsBuffer.hasCommitted = false;
    }
    m_target = nullptr;
#if USE(SKIA)
    if (m_skiaTarget) {
        m_skiaTarget->invalidate();
        m_skiaTarget = nullptr;
    }
#endif
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
    assertIsHeld(m_lock);
    m_pendingState.position = WTF::move(position);
}

void CoordinatedPlatformLayer::setPositionForScrolling(const FloatPoint& position)
{
    Locker locker { m_lock };
    m_pendingState.positionForScrolling = position;
}

const FloatPoint& CoordinatedPlatformLayer::position() const
{
    assertIsHeld(m_lock);
    return m_position;
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
    return m_position - toFloatSize(m_anchorPoint.xy()) * m_size;
}

void CoordinatedPlatformLayer::setBoundsOrigin(const FloatPoint& origin)
{
    assertIsHeld(m_lock);
    m_pendingState.boundsOrigin = origin;
}

void CoordinatedPlatformLayer::setBoundsOriginForScrolling(const FloatPoint& origin)
{
    Locker locker { m_lock };
    m_pendingState.boundsOriginForScrolling = origin;
}

const FloatPoint& CoordinatedPlatformLayer::boundsOrigin() const
{
    assertIsHeld(m_lock);
    return m_boundsOrigin;
}

void CoordinatedPlatformLayer::setAnchorPoint(FloatPoint3D&& point)
{
    assertIsHeld(m_lock);
    if (m_anchorPoint == point)
        return;

    m_anchorPoint = WTF::move(point);
    m_pendingChanges.add(Change::AnchorPoint);
    notifyCompositionRequired();
}

const FloatPoint3D& CoordinatedPlatformLayer::anchorPoint() const
{
    assertIsHeld(m_lock);
    return m_anchorPoint;
}

void CoordinatedPlatformLayer::setSize(FloatSize&& size)
{
    assertIsHeld(m_lock);
    if (m_size == size)
        return;

    m_size = WTF::move(size);
    m_pendingChanges.add(Change::Size);
    notifyCompositionRequired();
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
    assertIsHeld(m_lock);
    if (m_transform == matrix)
        return;

    m_transform = matrix;
    m_pendingChanges.add(Change::Transform);
    notifyCompositionRequired();
}

const TransformationMatrix& CoordinatedPlatformLayer::transform() const
{
    assertIsHeld(m_lock);
    return m_transform;
}

void CoordinatedPlatformLayer::setChildrenTransform(const TransformationMatrix& matrix)
{
    assertIsHeld(m_lock);
    if (m_childrenTransform == matrix)
        return;

    m_childrenTransform = matrix;
    m_pendingChanges.add(Change::ChildrenTransform);
    notifyCompositionRequired();
}

const TransformationMatrix& CoordinatedPlatformLayer::childrenTransform() const
{
    assertIsHeld(m_lock);
    return m_childrenTransform;
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
    assertIsHeld(m_lock);
    if (m_masksToBounds == masksToBounds)
        return;

    m_masksToBounds = masksToBounds;
    m_pendingChanges.add(Change::MasksToBounds);
    damageWholeLayer();
    notifyCompositionRequired();
}

bool CoordinatedPlatformLayer::masksToBounds() const
{
    assertIsHeld(m_lock);
    return m_masksToBounds;
}

void CoordinatedPlatformLayer::setPreserves3D(bool preserves3D)
{
    assertIsHeld(m_lock);
    if (m_preserves3D == preserves3D)
        return;

    m_preserves3D = preserves3D;
    m_pendingChanges.add(Change::Preserves3D);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setBackfaceVisibility(bool backfaceVisibility)
{
    assertIsHeld(m_lock);
    if (m_backfaceVisibility == backfaceVisibility)
        return;

    m_backfaceVisibility = backfaceVisibility;
    m_pendingChanges.add(Change::BackfaceVisibility);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setOpacity(float opacity)
{
    assertIsHeld(m_lock);
    if (m_opacity == opacity)
        return;

    m_opacity = opacity;
    m_pendingChanges.add(Change::Opacity);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setBlendMode(BlendMode blendMode)
{
    assertIsHeld(m_lock);
    if (m_blendMode == blendMode)
        return;

    m_blendMode = blendMode;
    m_pendingChanges.add(Change::BlendMode);
    damageWholeLayer();
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsVisible(bool contentsVisible)
{
    assertIsHeld(m_lock);
    if (m_contentsVisible == contentsVisible)
        return;

    m_contentsVisible = contentsVisible;
    m_pendingChanges.add(Change::ContentsVisible);
    damageWholeLayer();
    notifyCompositionRequired();
}

bool CoordinatedPlatformLayer::contentsVisible() const
{
    assertIsHeld(m_lock);
    return m_contentsVisible;
}

void CoordinatedPlatformLayer::setContentsOpaque(bool contentsOpaque)
{
    assertIsHeld(m_lock);
    if (m_contentsOpaque == contentsOpaque)
        return;

    m_contentsOpaque = contentsOpaque;
    m_pendingChanges.add(Change::ContentsOpaque);
    // FIXME: request a full repaint?
    damageWholeLayer();
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsRect(const FloatRect& contentsRect)
{
    assertIsHeld(m_lock);
    if (m_contentsRect == contentsRect)
        return;

    m_contentsRect = contentsRect;
    m_pendingChanges.add(Change::ContentsRect);
    damageWholeLayer();
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsRectClipsDescendants(bool contentsRectClipsDescendants)
{
    assertIsHeld(m_lock);
    if (m_contentsRectClipsDescendants == contentsRectClipsDescendants)
        return;

    m_contentsRectClipsDescendants = contentsRectClipsDescendants;
    m_pendingChanges.add(Change::ContentsRectClipsDescendants);
    damageWholeLayer();
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsClippingRect(const FloatRoundedRect& contentsClippingRect)
{
    assertIsHeld(m_lock);
    if (m_contentsClippingRect == contentsClippingRect)
        return;

    m_contentsClippingRect = contentsClippingRect;
    m_pendingChanges.add(Change::ContentsClippingRect);
    damageWholeLayer();
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsScale(float contentsScale)
{
    assertIsMainThread();
    assertIsHeld(m_lock);
    if (m_contentsScale == contentsScale)
        return;

    m_contentsScale = contentsScale;
    m_needsTilesUpdate = true;
    notifyCompositionRequired();
}

float CoordinatedPlatformLayer::contentsScale() const
{
    assertIsHeld(m_lock);
    return m_contentsScale;
}

void CoordinatedPlatformLayer::setContentsBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&& buffer, std::optional<Damage>&& dirtyRegion, RequireComposition requireComposition)
{
    assertIsHeld(m_lock);
    if (!buffer && !m_contentsBuffer.pending && !m_contentsBuffer.hasCommitted)
        return;

    m_contentsBuffer.pending = WTF::move(buffer);
    m_pendingChanges.add(Change::ContentsBuffer);
#if ENABLE(DAMAGE_TRACKING)
    if (dirtyRegion)
        addDamage(WTF::move(*dirtyRegion));
    else
        damageWholeLayer();
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
    if (!m_contentsBuffer.hasCommitted)
        return;

    m_contentsBuffer.pending = nullptr;

#if USE(SKIA)
    if (m_skiaTarget) {
        if (auto* buffer = m_skiaTarget->contentsBuffer()) {
            if (is<CoordinatedPlatformLayerBufferVideo>(*buffer))
                m_contentsBuffer.pending = downcast<CoordinatedPlatformLayerBufferVideo>(*buffer).copyBuffer();
            m_contentsBuffer.hasCommitted = !!m_contentsBuffer.pending;
            m_skiaTarget->setContentsBuffer(WTF::move(m_contentsBuffer.pending));
        }
        return;
    }
#endif
    if (is<CoordinatedPlatformLayerBufferVideo>(*m_contentsBuffer.committed))
        m_contentsBuffer.pending = downcast<CoordinatedPlatformLayerBufferVideo>(*m_contentsBuffer.committed).copyBuffer();
    m_contentsBuffer.committed = WTF::move(m_contentsBuffer.pending);
    m_contentsBuffer.hasCommitted = !!m_contentsBuffer.committed;
    ensureTarget().setContentsLayer(m_contentsBuffer.committed.get());
}
#endif

void CoordinatedPlatformLayer::setContentsImage(NativeImage* image)
{
    assertIsHeld(m_lock);
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
    damageWholeLayer();
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsColor(const Color& color)
{
    assertIsHeld(m_lock);
    if (m_contentsColor == color)
        return;

    m_contentsColor = color;
    m_pendingChanges.add(Change::ContentsColor);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsTileSize(const FloatSize& contentsTileSize)
{
    assertIsHeld(m_lock);
    if (m_contentsTileSize == contentsTileSize)
        return;

    m_contentsTileSize = contentsTileSize;
    m_pendingChanges.add(Change::ContentsTiling);
    damageWholeLayer();
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setContentsTilePhase(const FloatSize& contentsTilePhase)
{
    assertIsHeld(m_lock);
    if (m_contentsTilePhase == contentsTilePhase)
        return;

    m_contentsTilePhase = contentsTilePhase;
    m_pendingChanges.add(Change::ContentsTiling);
    damageWholeLayer();
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setDirtyRegion(Damage&& damage)
{
    assertIsMainThread();
    assertIsHeld(m_lock);
    auto dirtyRegion = damage.rects();
    if (m_dirtyRegion != dirtyRegion) {
        m_dirtyRegion = WTF::move(dirtyRegion);
        notifyCompositionRequired();
    }

#if ENABLE(DAMAGE_TRACKING)
    addDamage(WTF::move(damage));
#endif
}

#if ENABLE(DAMAGE_TRACKING)
void CoordinatedPlatformLayer::addDamage(Damage&& damage)
{
    assertIsHeld(m_lock);
    if (!m_damage)
        m_damage = WTF::move(damage);
    else
        m_damage->add(damage);
    m_pendingChanges.add(Change::Damage);
}
#endif

void CoordinatedPlatformLayer::damageWholeLayer()
{
#if ENABLE(DAMAGE_TRACKING)
    assertIsHeld(m_lock);
    // An empty Damage rejects everything added to it later, so it must never become the layer's damage.
    if (!m_damagePropagationEnabled || m_size.isEmpty())
        return;

    addDamage(Damage { m_size, Damage::Mode::Full });
#endif
}

void CoordinatedPlatformLayer::setFilters(const FilterOperations& filters)
{
    assertIsHeld(m_lock);
    if (m_filters == filters)
        return;

    m_filters = filters;
    m_pendingChanges.add(Change::Filters);
    damageWholeLayer();
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setMask(CoordinatedPlatformLayer* mask)
{
    assertIsHeld(m_lock);
    if (m_mask == mask)
        return;

    m_mask = mask;
    m_pendingChanges.add(Change::Mask);
    damageWholeLayer();
    notifyCompositionRequired();
}

CoordinatedPlatformLayer* CoordinatedPlatformLayer::mask() const
{
    assertIsHeld(m_lock);
    return m_mask;
}

void CoordinatedPlatformLayer::setReplica(CoordinatedPlatformLayer* replica)
{
    assertIsHeld(m_lock);
    if (m_replica == replica)
        return;

    m_replica = replica;
    m_pendingChanges.add(Change::Replica);
    damageWholeLayer();
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setBackdrop(CoordinatedPlatformLayer* backdrop)
{
    assertIsHeld(m_lock);
    if (m_backdrop == backdrop)
        return;

    m_backdrop = backdrop;
    notifyBackdropFiltersChanged();
}

void CoordinatedPlatformLayer::notifyBackdropFiltersChanged()
{
    assertIsHeld(m_lock);
    m_pendingChanges.add(Change::Backdrop);
    damageWholeLayer();
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setBackdropRect(const FloatRoundedRect& backdropRect)
{
    assertIsHeld(m_lock);
    if (m_backdropRect == backdropRect)
        return;

    m_backdropRect = backdropRect;
    m_pendingChanges.add(Change::BackdropRect);
    damageWholeLayer();
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setIsBackdropRoot(bool isBackdropRoot)
{
    assertIsHeld(m_lock);
    if (m_isBackdropRoot == isBackdropRoot)
        return;

    m_isBackdropRoot = isBackdropRoot;
    m_pendingChanges.add(Change::BackdropRoot);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setAnimations(const TextureMapperAnimations& animations)
{
    assertIsHeld(m_lock);
    m_animations = animations;
    m_pendingChanges.add(Change::Animations);
    notifyCompositionRequired();
}

RefPtr<CoordinatedPlatformLayer> CoordinatedPlatformLayer::parent() const
{
    assertIsHeld(m_lock);
    return m_parent;
}

void CoordinatedPlatformLayer::setChildren(Vector<Ref<CoordinatedPlatformLayer>>&& children)
{
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

    m_pendingChanges.add(Change::Children);
    notifyCompositionRequired();
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
    assertIsHeld(m_lock);
    m_clipPath.path = path;
    m_clipPath.windRule = windRule;
    m_pendingChanges.add(Change::ClipPath);
    damageWholeLayer();
}

void CoordinatedPlatformLayer::setDebugBorder(Color&& borderColor, float borderWidth)
{
    assertIsHeld(m_lock);
    if (m_debugBorderColor == borderColor && m_debugBorderWidth == borderWidth)
        return;

    m_debugBorderColor = WTF::move(borderColor);
    m_debugBorderWidth = borderWidth;
    m_pendingChanges.add(Change::DebugIndicators);
    notifyCompositionRequired();
}

void CoordinatedPlatformLayer::setShowRepaintCounter(bool showRepaintCounter)
{
    assertIsMainThread();
    assertIsHeld(m_lock);
    if ((m_repaintCount != -1 && showRepaintCounter) || (m_repaintCount == -1 && !showRepaintCounter))
        return;

    m_repaintCount = showRepaintCounter ? m_owner->repaintCount() : -1;
    m_pendingChanges.add(Change::DebugIndicators);
    notifyCompositionRequired();
}

bool CoordinatedPlatformLayer::needsBackingStore() const
{
    assertIsMainThread();
    assertIsHeld(m_lock);
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
    assertIsMainThread();

    if (m_dirtyRegion.isEmpty() && !m_pendingTilesCreation && !m_needsTilesUpdate)
        return;

    FloatSize size;
    float contentsScale;
    bool contentsOpaque;
    RefPtr<CoordinatedBackingStoreProxy> backingStoreProxy;
    {
        Locker locker { m_lock };
        if (!m_backingStoreProxy)
            return;

        size = m_size;
        contentsScale = m_contentsScale;
        contentsOpaque = m_contentsOpaque;
        backingStoreProxy = m_backingStoreProxy;
    }

    Damage damage(size, Damage::Mode::Rectangles);
    auto updateResult = backingStoreProxy->updateIfNeeded(m_transformedVisibleRect, size, m_visibleRect, contentsScale, contentsOpaque, m_pendingTilesCreation || m_needsTilesUpdate, m_dirtyRegion, damage, *this);
    m_dirtyRegion.clear();
    m_needsTilesUpdate = false;
    m_pendingTilesCreation = updateResult.contains(CoordinatedBackingStoreProxy::UpdateResult::TilesPending);

    bool tilesChanged = updateResult.contains(CoordinatedBackingStoreProxy::UpdateResult::TilesChanged);
    {
        Locker locker { m_lock };
#if ENABLE(DAMAGE_TRACKING)
        addDamage(WTF::move(damage));
#endif

        if (tilesChanged) {
            if (m_repaintCount != -1 && updateResult.contains(CoordinatedBackingStoreProxy::UpdateResult::BuffersChanged)) {
                m_repaintCount = m_owner->incrementRepaintCount();
                m_pendingChanges.add(Change::DebugIndicators);
            }
        }
    }

    if (tilesChanged)
        notifyCompositionRequired();
}

void CoordinatedPlatformLayer::updateContents(bool affectedByTransformAnimation)
{
    assertIsMainThread();
    assertIsHeld(m_lock);

    if (needsBackingStore()) {
        if (!m_backingStoreProxy) {
            m_backingStoreProxy = CoordinatedBackingStoreProxy::create();
            m_backingStoreProxy->setAffectedByTransformAnimation(affectedByTransformAnimation);
            m_needsTilesUpdate = true;
            m_pendingChanges.add(Change::BackingStore);
        } else {
            bool wasAffectedByTransformAnimation = !!m_backingStoreProxy->animatedBackingStoreClient();
            if (wasAffectedByTransformAnimation != affectedByTransformAnimation) {
                m_backingStoreProxy->setAffectedByTransformAnimation(affectedByTransformAnimation);
                m_pendingChanges.add(Change::BackingStore);
            }
        }
    } else {
        if (m_backingStoreProxy) {
            m_backingStoreProxy->invalidate();
            m_backingStoreProxy = nullptr;
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
    if (m_backingStoreProxy) {
        m_backingStoreProxy->invalidate();
        m_backingStoreProxy = nullptr;
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

#if USE(SKIA)
sk_sp<GrContextThreadSafeProxy> CoordinatedPlatformLayer::threadSafeGrContext() const
{
    if (!m_client)
        return nullptr;

    return m_client->paintingEngine().threadSafeGrContext();
}
#endif

void CoordinatedPlatformLayer::waitUntilPaintingComplete()
{
    Locker locker { m_lock };
    if (m_backingStoreProxy)
        m_backingStoreProxy->waitUntilPaintingComplete();
}

void CoordinatedPlatformLayer::flushPendingState()
{
    Locker locker { m_lock };
    if (!m_pendingState.position && !m_pendingState.boundsOrigin && !m_pendingState.positionForScrolling && !m_pendingState.boundsOriginForScrolling)
        return;

    std::optional<FloatPoint> position;
    if (m_pendingState.positionForScrolling) {
        m_pendingState.position = std::nullopt;
        position = *std::exchange(m_pendingState.positionForScrolling, std::nullopt);
    } else if (m_pendingState.position)
        position = *std::exchange(m_pendingState.position, std::nullopt);

    std::optional<FloatPoint> boundsOrigin;
    if (m_pendingState.boundsOriginForScrolling) {
        m_pendingState.boundsOrigin = std::nullopt;
        boundsOrigin = *std::exchange(m_pendingState.boundsOriginForScrolling, std::nullopt);
    } else if (m_pendingState.boundsOrigin)
        boundsOrigin = *std::exchange(m_pendingState.boundsOrigin, std::nullopt);

    bool requireComposition = false;
    if (position && m_position != *position) {
        m_position = *position;
        m_pendingChanges.add(Change::Position);
        requireComposition = true;
    }

    if (boundsOrigin && m_boundsOrigin != boundsOrigin) {
        m_boundsOrigin = *boundsOrigin;
        m_pendingChanges.add(Change::BoundsOrigin);
        requireComposition = true;
    }

    if (requireComposition)
        notifyCompositionRequired();
}

void CoordinatedPlatformLayer::flushPositionChanges(const OptionSet<CompositionReason>& reasons, bool useSkiaTarget)
{
    ASSERT(!isMainThread());
    if (!reasons.containsAny({ CompositionReason::RenderingUpdate, CompositionReason::AsyncScrolling }))
        return;

    Locker locker { m_lock };
    if (!m_pendingChanges.containsAny({ Change::Position, Change::BoundsOrigin }))
        return;

    auto applyPositionChanges = [this](auto& layer) {
        assertIsHeld(m_lock);
        if (m_pendingChanges.contains(Change::Position)) {
            layer.setPosition(m_position);
            m_pendingChanges.remove(Change::Position);
        }

        if (m_pendingChanges.contains(Change::BoundsOrigin)) {
            layer.setBoundsOrigin(m_boundsOrigin);
            m_pendingChanges.remove(Change::BoundsOrigin);
        }
    };

#if USE(SKIA)
    if (useSkiaTarget) {
        applyPositionChanges(ensureSkiaTarget());
        return;
    }
#else
    UNUSED_PARAM(useSkiaTarget);
#endif

    applyPositionChanges(ensureTarget());
}

void CoordinatedPlatformLayer::flushCompositingState(const OptionSet<CompositionReason>& reasons, bool useSkiaTarget)
{
    ASSERT(!isMainThread());
    Locker locker { m_lock };
    if (m_pendingChanges.isEmpty() && (!reasons.contains(CompositionReason::RenderingUpdate) || !m_backingStoreProxy))
        return;

#if USE(SKIA)
    if (useSkiaTarget) {
        flushCompositingStateOnSkiaTarget(reasons, ensureSkiaTarget());
        return;
    }
#else
    UNUSED_PARAM(useSkiaTarget);
#endif

    flushCompositingStateOnTarget(reasons, ensureTarget());
}

void CoordinatedPlatformLayer::flushCompositingStateOnTarget(const OptionSet<CompositionReason>& reasons, TextureMapperLayer& layer)
{
    assertIsHeld(m_lock);
    if (reasons.containsAny({ CompositionReason::RenderingUpdate, CompositionReason::AsyncScrolling })) {
        if (m_pendingChanges.contains(Change::ContentsRect)) {
            layer.setContentsRect(m_contentsRect);
            m_pendingChanges.remove(Change::ContentsRect);
        }

        if (m_pendingChanges.contains(Change::ContentsClippingRect)) {
            layer.setContentsClippingRect(m_contentsClippingRect);
            m_pendingChanges.remove(Change::ContentsClippingRect);
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

                if (auto* animatedBackingStoreClient = m_backingStoreProxy->animatedBackingStoreClient())
                    layer.setAnimatedBackingStoreClient(animatedBackingStoreClient);
            } else {
                layer.setBackingStore(nullptr);
                layer.setAnimatedBackingStoreClient(nullptr);
                m_backingStore = nullptr;
            }
            m_pendingChanges.remove(Change::BackingStore);
        }

        if (m_pendingChanges.contains(Change::ContentsImage)) {
            m_imageBackingStore.committed = m_imageBackingStore.current;
            m_pendingChanges.remove(Change::ContentsImage);
        }

        if (m_pendingChanges.contains(Change::ContentsVisible)) {
            layer.setContentsVisible(m_contentsVisible);
            m_pendingChanges.remove(Change::ContentsVisible);
        }

        if (m_pendingChanges.contains(Change::ContentsOpaque)) {
            layer.setContentsOpaque(m_contentsOpaque);
            m_pendingChanges.remove(Change::ContentsOpaque);
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
        }
    }

    if (reasons.containsAny({ CompositionReason::RenderingUpdate, CompositionReason::VideoFrame, CompositionReason::AsyncScrolling })) {
        if (m_pendingChanges.contains(Change::ContentsBuffer)) {
            m_contentsBuffer.committed = WTF::move(m_contentsBuffer.pending);
            m_contentsBuffer.hasCommitted = !!m_contentsBuffer.committed;
            m_pendingChanges.remove(Change::ContentsBuffer);
        }

        if (m_contentsBuffer.committed)
            layer.setContentsLayer(m_contentsBuffer.committed.get());
        else if (m_imageBackingStore.committed) {
            if (reasons.containsAny({ CompositionReason::RenderingUpdate, CompositionReason::AsyncScrolling }))
                layer.setContentsLayer(m_imageBackingStore.committed->buffer());
        } else
            layer.setContentsLayer(nullptr);
    }
}

#if USE(SKIA)
void CoordinatedPlatformLayer::flushCompositingStateOnSkiaTarget(const OptionSet<CompositionReason>& reasons, SkiaCompositingLayer& layer)
{
    assertIsHeld(m_lock);
    if (reasons.containsAny({ CompositionReason::RenderingUpdate, CompositionReason::AsyncScrolling })) {
        if (m_pendingChanges.contains(Change::ContentsRect)) {
            layer.setContentsRect(m_contentsRect);
            m_pendingChanges.remove(Change::ContentsRect);
        }

        if (m_pendingChanges.contains(Change::ContentsClippingRect)) {
            layer.setContentsClippingRect(m_contentsClippingRect);
            m_pendingChanges.remove(Change::ContentsClippingRect);
        }

        if (m_pendingChanges.contains(Change::ContentsImage)) {
            layer.setImageBackingStore(m_imageBackingStore.current);
            m_pendingChanges.remove(Change::ContentsImage);
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

        if (m_pendingChanges.contains(Change::BlendMode)) {
            layer.setBlendMode(m_blendMode);
            m_pendingChanges.remove(Change::BlendMode);
        }

        if (m_pendingChanges.contains(Change::BackingStore)) {
            layer.setUseBackingStore(!!m_backingStoreProxy, m_backingStoreProxy ? m_backingStoreProxy->animatedBackingStoreClient() : nullptr);
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

        if (m_pendingChanges.contains(Change::ContentsRectClipsDescendants)) {
            layer.setContentsRectClipsDescendants(m_contentsRectClipsDescendants);
            m_pendingChanges.remove(Change::ContentsRectClipsDescendants);
        }

        if (m_pendingChanges.contains(Change::ContentsTiling)) {
            layer.setContentsTiling(m_contentsTileSize, m_contentsTilePhase);
            m_pendingChanges.remove(Change::ContentsTiling);
        }

        if (m_pendingChanges.contains(Change::ContentsColor)) {
            layer.setContentsSolidColor(m_contentsColor);
            m_pendingChanges.remove(Change::ContentsColor);
        }

        if (m_pendingChanges.contains(Change::ClipPath)) {
            auto clipPath = *m_clipPath.path.platformPath();
            clipPath.setFillType(m_clipPath.windRule == WindRule::EvenOdd ? SkPathFillType::kEvenOdd : SkPathFillType::kWinding);
            layer.setClipPath(WTF::move(clipPath));
            m_pendingChanges.remove(Change::ClipPath);
        }

        if (m_pendingChanges.contains(Change::Filters)) {
            layer.setFilters(m_filters);
            m_pendingChanges.remove(Change::Filters);
        }

        if (m_pendingChanges.contains(Change::Mask)) {
            layer.setMask(m_mask ? RefPtr { &m_mask->ensureSkiaTarget() } : nullptr);
            m_pendingChanges.remove(Change::Mask);
        }

        if (m_pendingChanges.contains(Change::Replica)) {
            layer.setReplica(m_replica ? RefPtr { &m_replica->ensureSkiaTarget() } : nullptr);
            m_pendingChanges.remove(Change::Replica);
        }

        // FIXME: stop creating a layer for backdrop filters when switching to SkiaCompositingLayer.
        if (m_pendingChanges.contains(Change::Backdrop) && !m_backdrop) {
            layer.setBackdropFilters(FilterOperations());
            m_pendingChanges.remove(Change::Backdrop);
        } else if (m_backdrop) {
            Locker locker { m_backdrop->lock() };
            if (m_pendingChanges.contains(Change::Backdrop) || m_backdrop->m_pendingChanges.contains(Change::Filters)) {
                layer.setBackdropFilters(m_backdrop->m_filters);
                m_pendingChanges.remove(Change::Backdrop);
                m_backdrop->m_pendingChanges.remove(Change::Filters);
            }
        }

        if (m_pendingChanges.contains(Change::BackdropRect)) {
            layer.setBackdropFiltersRect(m_backdropRect);
            m_pendingChanges.remove(Change::BackdropRect);
        }

        if (m_pendingChanges.contains(Change::BackdropRoot)) {
            layer.setIsBackdropRoot(m_isBackdropRoot);
            m_pendingChanges.remove(Change::BackdropRoot);
        }

        if (m_pendingChanges.contains(Change::Animations)) {
            layer.setAnimations(m_animations);
            m_pendingChanges.remove(Change::Animations);
        }

        if (m_pendingChanges.contains(Change::DebugIndicators)) {
            Color color;
            std::optional<float> width;
            if (m_debugBorderColor.isVisible()) {
                color = m_debugBorderColor;
                width = m_debugBorderWidth;
            }
            std::optional<unsigned> repaintCount;
            if (m_repaintCount != -1)
                repaintCount = m_repaintCount;

            layer.setDebugIndicators(WTF::move(color), width, repaintCount);
            m_pendingChanges.remove(Change::DebugIndicators);
        }

        if (m_pendingChanges.contains(Change::Children)) {
            layer.setChildren(WTF::map(m_children, [](auto& child) {
                return Ref { child->ensureSkiaTarget() };
            }));
            m_pendingChanges.remove(Change::Children);
        }

        if (m_backingStoreProxy)
            layer.updateBackingStore(m_backingStoreProxy->takePendingUpdate(), m_contentsScale);
    }

    if (reasons.containsAny({ CompositionReason::RenderingUpdate, CompositionReason::VideoFrame, CompositionReason::AsyncScrolling })) {
#if ENABLE(DAMAGE_TRACKING)
        if (m_pendingChanges.contains(Change::Damage)) {
            ASSERT(m_damage.has_value());
            layer.addDamage(*std::exchange(m_damage, std::nullopt));
            m_pendingChanges.remove(Change::Damage);
        }
#endif
        if (m_pendingChanges.contains(Change::ContentsBuffer)) {
            m_contentsBuffer.hasCommitted = !!m_contentsBuffer.pending;
            layer.setContentsBuffer(WTF::move(m_contentsBuffer.pending));
            m_pendingChanges.remove(Change::ContentsBuffer);
        }
    }
}
#endif // USE(SKIA)

bool CoordinatedPlatformLayer::hasPendingBackingStoreTileUpdates() const
{
    ASSERT(!isMainThread());

#if USE(SKIA)
    if (m_skiaTarget)
        return m_skiaTarget->hasPendingBackingStoreTileUpdates();
#endif

    Locker locker { m_lock };
    if (m_backingStore)
        return m_backingStore->hasPendingUpdates();

    return false;
}

void CoordinatedPlatformLayer::processPendingBackingStoreTileUpdates()
{
    ASSERT(!isMainThread());

#if USE(SKIA)
    if (m_skiaTarget) {
        m_skiaTarget->processPendingTileUpdates();
        return;
    }
#endif

    Locker locker { m_lock };
    if (m_backingStore)
        m_backingStore->processPendingUpdates();
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
