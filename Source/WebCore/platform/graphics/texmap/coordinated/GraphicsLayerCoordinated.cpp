/*
 * Copyright (C) 2024 Igalia S.L.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Company 100, Inc.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
#include "GraphicsLayerCoordinated.h"

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedImageBackingStore.h"
#include "CoordinatedPlatformLayer.h"
#include "CoordinatedPlatformLayerBuffer.h"
#include "CoordinatedPlatformLayerBufferProxy.h"
#include "FloatQuad.h"
#include "GraphicsLayerAsyncContentsDisplayDelegateCoordinated.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "GraphicsLayerFactory.h"
#include "Image.h"
#include "NativeImage.h"
#include <wtf/Locker.h>

namespace WebCore {

static constexpr uint32_t s_maxDamageRectanglesForHighResolutionDamage = 32;

Ref<GraphicsLayer> GraphicsLayer::create(GraphicsLayerFactory* factory, GraphicsLayerClient& client, Type layerType)
{
    if (factory)
        return factory->createGraphicsLayer(layerType, client);

    return adoptRef(*new GraphicsLayerCoordinated(layerType, client, CoordinatedPlatformLayer::create()));
}

GraphicsLayerCoordinated::GraphicsLayerCoordinated(Type layerType, GraphicsLayerClient& client, Ref<CoordinatedPlatformLayer>&& platformLayer)
    : GraphicsLayer(layerType, client)
    , m_platformLayer(WTFMove(platformLayer))
{
    m_platformLayer->setOwner(this);
    noteLayerPropertyChanged({ Change::ContentsScale, Change::ContentsVisible }, ScheduleFlush::Yes);
}

GraphicsLayerCoordinated::~GraphicsLayerCoordinated()
{
    if (m_contentsBufferProxy)
        m_contentsBufferProxy->setTargetLayer(nullptr);
    m_platformLayer->setOwner(nullptr);
    if (m_parent)
        downcast<GraphicsLayerCoordinated>(*m_parent).noteLayerPropertyChanged(Change::Children, ScheduleFlush::Yes);
    willBeDestroyed();
}

std::optional<PlatformLayerIdentifier> GraphicsLayerCoordinated::primaryLayerID() const
{
    return m_platformLayer->id();
}

void GraphicsLayerCoordinated::setNeedsDisplay()
{
    if (!m_drawsContent || !m_contentsVisible || m_size.isEmpty())
        return;

    if (m_dirtyRegion) {
        if (m_dirtyRegion->mode() == Damage::Mode::Full)
            return;

        m_dirtyRegion->makeFull();
    } else
        m_dirtyRegion = Damage(m_size, Damage::Mode::Full);

    noteLayerPropertyChanged(Change::DirtyRegion, ScheduleFlush::Yes);
    addRepaintRect({ { }, m_size });
}

void GraphicsLayerCoordinated::setNeedsDisplayInRect(const FloatRect& initialRect, ShouldClipToLayer shouldClip)
{
    if (!m_drawsContent || !m_contentsVisible || m_size.isEmpty())
        return;

    if (m_dirtyRegion && m_dirtyRegion->mode() == Damage::Mode::Full)
        return;

    auto rect = initialRect;
    if (shouldClip == ShouldClipToLayer::Clip)
        rect.intersect({ { }, m_size });

    if (rect.isEmpty())
        return;

    addRepaintRect(rect);

    if (!m_dirtyRegion)
        m_dirtyRegion = Damage(m_size, Damage::Mode::Rectangles, s_maxDamageRectanglesForHighResolutionDamage);

    if (m_dirtyRegion->add(rect))
        noteLayerPropertyChanged(Change::DirtyRegion, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setPosition(const FloatPoint& position)
{
    if (m_position == position)
        return;

    GraphicsLayer::setPosition(position);
    noteLayerPropertyChanged(Change::Geometry, ScheduleFlush::Yes);
    setNeedsUpdateLayerTransform();
}

void GraphicsLayerCoordinated::syncPosition(const FloatPoint& position)
{
    if (m_position == position)
        return;

    GraphicsLayer::syncPosition(position);

    // Ensure future flushes will recompute the coverage rect and update tiling.
    noteLayerPropertyChanged(Change::TileCoverage, ScheduleFlush::No);
    setNeedsUpdateLayerTransform();
}

void GraphicsLayerCoordinated::setBoundsOrigin(const FloatPoint& origin)
{
    if (m_boundsOrigin == origin)
        return;

    GraphicsLayer::setBoundsOrigin(origin);
    noteLayerPropertyChanged(Change::Geometry, ScheduleFlush::Yes);
    setNeedsUpdateLayerTransform();
}

void GraphicsLayerCoordinated::syncBoundsOrigin(const FloatPoint& origin)
{
    if (m_boundsOrigin == origin)
        return;

    GraphicsLayer::syncBoundsOrigin(origin);

    // Ensure future flushes will recompute the coverage rect and update tiling.
    noteLayerPropertyChanged(Change::TileCoverage, ScheduleFlush::No);
    setNeedsUpdateLayerTransform();
}

void GraphicsLayerCoordinated::setAnchorPoint(const FloatPoint3D& point)
{
    if (m_anchorPoint == point)
        return;

    GraphicsLayer::setAnchorPoint(point);
    noteLayerPropertyChanged(Change::Geometry, ScheduleFlush::Yes);
    setNeedsUpdateLayerTransform();
}

void GraphicsLayerCoordinated::setSize(const FloatSize& size)
{
    if (m_size == size)
        return;

    GraphicsLayer::setSize(size);
    if (m_maskLayer)
        m_maskLayer->setSize(m_size);
    noteLayerPropertyChanged(Change::Geometry, ScheduleFlush::Yes);
    setNeedsUpdateLayerTransform();
}

#if ENABLE(SCROLLING_THREAD)
void GraphicsLayerCoordinated::setScrollingNodeID(std::optional<ScrollingNodeID> nodeID)
{
    if (m_scrollingNodeID == nodeID)
        return;

    GraphicsLayer::setScrollingNodeID(nodeID);
    noteLayerPropertyChanged(Change::ScrollingNode, ScheduleFlush::Yes);
}
#endif

void GraphicsLayerCoordinated::setTransform(const TransformationMatrix& matrix)
{
    if (transform() == matrix)
        return;

    GraphicsLayer::setTransform(matrix);
    noteLayerPropertyChanged(Change::Transform, ScheduleFlush::Yes);
    setNeedsUpdateLayerTransform();
}

void GraphicsLayerCoordinated::setChildrenTransform(const TransformationMatrix& matrix)
{
    if (childrenTransform() == matrix)
        return;

    GraphicsLayer::setChildrenTransform(matrix);
    noteLayerPropertyChanged(Change::ChildrenTransform, ScheduleFlush::Yes);
    setNeedsUpdateLayerTransform();
}

void GraphicsLayerCoordinated::setDrawsContent(bool drawsContent)
{
    if (m_drawsContent == drawsContent)
        return;

    GraphicsLayer::setDrawsContent(drawsContent);
    noteLayerPropertyChanged({ Change::DrawsContent, Change::DebugIndicators }, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setMasksToBounds(bool masksToBounds)
{
    if (m_masksToBounds == masksToBounds)
        return;

    GraphicsLayer::setMasksToBounds(masksToBounds);
    noteLayerPropertyChanged({ Change::MasksToBounds, Change::DebugIndicators }, ScheduleFlush::Yes);
    setNeedsUpdateLayerTransform();
}

void GraphicsLayerCoordinated::setPreserves3D(bool preserves3D)
{
    if (m_preserves3D == preserves3D)
        return;

    GraphicsLayer::setPreserves3D(preserves3D);
    noteLayerPropertyChanged(Change::Preserves3D, ScheduleFlush::Yes);
    setNeedsUpdateLayerTransform();
}

void GraphicsLayerCoordinated::setBackfaceVisibility(bool backfaceVisibility)
{
    if (m_backfaceVisibility == backfaceVisibility)
        return;

    GraphicsLayer::setBackfaceVisibility(backfaceVisibility);
    noteLayerPropertyChanged(Change::BackfaceVisibility, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setOpacity(float opacity)
{
    if (m_opacity == opacity)
        return;

    GraphicsLayer::setOpacity(opacity);
    noteLayerPropertyChanged(Change::Opacity, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setContentsVisible(bool contentsVisible)
{
    if (m_contentsVisible == contentsVisible)
        return;

    GraphicsLayer::setContentsVisible(contentsVisible);
    if (m_maskLayer)
        m_maskLayer->setContentsVisible(m_contentsVisible);
    noteLayerPropertyChanged(Change::ContentsVisible, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setContentsOpaque(bool contentsOpaque)
{
    if (m_contentsOpaque == contentsOpaque)
        return;

    GraphicsLayer::setContentsOpaque(contentsOpaque);
    noteLayerPropertyChanged(Change::ContentsOpaque, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setContentsRect(const FloatRect& contentsRect)
{
    if (m_contentsRect == contentsRect)
        return;

    GraphicsLayer::setContentsRect(contentsRect);
    noteLayerPropertyChanged(Change::ContentsRect, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setContentsRectClipsDescendants(bool contentsRectClipsDescendants)
{
    if (m_contentsRectClipsDescendants == contentsRectClipsDescendants)
        return;

    GraphicsLayer::setContentsRectClipsDescendants(contentsRectClipsDescendants);
    noteLayerPropertyChanged(Change::ContentsRectClipsDescendants, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setContentsTileSize(const FloatSize& contentsTileSize)
{
    if (m_contentsTileSize == contentsTileSize)
        return;

    GraphicsLayer::setContentsTileSize(contentsTileSize);
    noteLayerPropertyChanged(Change::ContentsTiling, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setContentsTilePhase(const FloatSize& contentsTilePhase)
{
    if (m_contentsTilePhase == contentsTilePhase)
        return;

    GraphicsLayer::setContentsTilePhase(contentsTilePhase);
    noteLayerPropertyChanged(Change::ContentsTiling, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setContentsClippingRect(const FloatRoundedRect& contentsClippingRect)
{
    if (m_contentsClippingRect == contentsClippingRect)
        return;

    GraphicsLayer::setContentsClippingRect(contentsClippingRect);
    noteLayerPropertyChanged(Change::ContentsClippingRect, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setContentsNeedsDisplay()
{
    if (m_contentsDisplayDelegate)
        noteLayerPropertyChanged(Change::ContentsBufferNeedsDisplay, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setContentsNeedsDisplayInRect(const FloatRect& rect)
{
    if (!m_contentsDisplayDelegate)
        return;

#if ENABLE(DAMAGE_TRACKING)
    if (!rect.isEmpty() && m_contentsVisible && !m_size.isEmpty()) {
        if (!m_contentsDirtyRegion)
            m_contentsDirtyRegion = Damage(m_size, Damage::Mode::Rectangles, s_maxDamageRectanglesForHighResolutionDamage);

        m_contentsDirtyRegion->add(rect);
    }
#else
    UNUSED_PARAM(rect);
#endif
    noteLayerPropertyChanged(Change::ContentsBufferNeedsDisplay, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setContentsToPlatformLayer(PlatformLayer* contentsLayer, ContentsLayerPurpose)
{
    if (m_contentsBufferProxy == contentsLayer)
        return;

    if (m_contentsBufferProxy)
        m_contentsBufferProxy->setTargetLayer(nullptr);

    m_contentsBufferProxy = contentsLayer;

    OptionSet<Change> change = { Change::ContentsBuffer };
    if (m_contentsBufferProxy) {
        m_contentsBufferProxy->setTargetLayer(m_platformLayer.ptr());
        m_contentsDisplayDelegate = nullptr;
        change.add(Change::ContentsBufferNeedsDisplay);
    }
    noteLayerPropertyChanged(change, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setContentsDisplayDelegate(RefPtr<GraphicsLayerContentsDisplayDelegate>&& delegate, ContentsLayerPurpose)
{
    if (m_contentsDisplayDelegate == delegate)
        return;

    m_contentsDisplayDelegate = WTFMove(delegate);

    OptionSet<Change> change = { Change::ContentsBuffer };
    if (m_contentsDisplayDelegate) {
        if (m_contentsBufferProxy) {
            m_contentsBufferProxy->setTargetLayer(nullptr);
            m_contentsBufferProxy = nullptr;
        }
        change.add(Change::ContentsBufferNeedsDisplay);
    }
    noteLayerPropertyChanged(change, ScheduleFlush::Yes);
}

RefPtr<GraphicsLayerAsyncContentsDisplayDelegate> GraphicsLayerCoordinated::createAsyncContentsDisplayDelegate(GraphicsLayerAsyncContentsDisplayDelegate* existing)
{
    if (existing) {
        static_cast<GraphicsLayerAsyncContentsDisplayDelegateCoordinated*>(existing)->updateGraphicsLayer(*this);
        return existing;
    }
    return GraphicsLayerAsyncContentsDisplayDelegateCoordinated::create(*this);
}

void GraphicsLayerCoordinated::setContentsToImage(Image* image)
{
    if (image) {
        auto nativeImage = image->currentNativeImage();
        if (!nativeImage)
            return;

        if (m_contentsImage && m_contentsImage->uniqueID() == nativeImage->uniqueID())
            return;

        m_contentsImage = WTFMove(nativeImage);
    } else {
        if (!m_contentsImage)
            return;
        m_contentsImage = nullptr;
    }
    noteLayerPropertyChanged(Change::ContentsImage, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setContentsToSolidColor(const Color& color)
{
    if (m_contentsColor == color)
        return;

    m_contentsColor = color;
    noteLayerPropertyChanged(Change::ContentsColor, ScheduleFlush::Yes);
}

bool GraphicsLayerCoordinated::usesContentsLayer() const
{
    // FIXME: convert CoordinatedImageBackingStore into a contents layer?
    return m_contentsBufferProxy || m_contentsDisplayDelegate || m_contentsImage;
}

bool GraphicsLayerCoordinated::setChildren(Vector<Ref<GraphicsLayer>>&& children)
{
    if (!GraphicsLayer::setChildren(WTFMove(children)))
        return false;

    noteLayerPropertyChanged(Change::Children, ScheduleFlush::Yes);
    return true;
}

void GraphicsLayerCoordinated::addChild(Ref<GraphicsLayer>&& layer)
{
    GraphicsLayer::addChild(WTFMove(layer));
    noteLayerPropertyChanged(Change::Children, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::addChildAtIndex(Ref<GraphicsLayer>&& layer, int index)
{
    GraphicsLayer::addChildAtIndex(WTFMove(layer), index);
    noteLayerPropertyChanged(Change::Children, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::addChildBelow(Ref<GraphicsLayer>&& layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(WTFMove(layer), sibling);
    noteLayerPropertyChanged(Change::Children, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::addChildAbove(Ref<GraphicsLayer>&& layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildAbove(WTFMove(layer), sibling);
    noteLayerPropertyChanged(Change::Children, ScheduleFlush::Yes);
}

bool GraphicsLayerCoordinated::replaceChild(GraphicsLayer* oldChild, Ref<GraphicsLayer>&& newChild)
{
    if (!GraphicsLayer::replaceChild(oldChild, WTFMove(newChild)))
        return false;

    noteLayerPropertyChanged(Change::Children, ScheduleFlush::Yes);
    return true;
}

void GraphicsLayerCoordinated::willModifyChildren()
{
    noteLayerPropertyChanged(Change::Children, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setEventRegion(EventRegion&& eventRegion)
{
    if (m_eventRegion == eventRegion)
        return;

    GraphicsLayer::setEventRegion(WTFMove(eventRegion));
    noteLayerPropertyChanged(Change::EventRegion, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::deviceOrPageScaleFactorChanged()
{
    noteLayerPropertyChanged(Change::ContentsScale, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::updateRootRelativeScale()
{
    // For CSS animations we could figure out the max scale level during the animation and only figure out the max content scale once.
    // For JS driven animation, we need to be more clever to keep the peformance as before. Ideas:
    // - only update scale factor when the change is 'significant' (to be defined, (orig - new)/orig > delta?)
    // - never update the scale factor when it gets smaller (unless we're under memory pressure) (or only periodically)
    // - ...
    // --> For now we disable this logic altogether, but allow to turn it on selectively (for LBSE)
    if (!m_shouldUpdateRootRelativeScaleFactor)
        return;

    auto computeMaxScaleFromTransform = [](const TransformationMatrix& transform) -> float {
        if (transform.isIdentityOrTranslation())
            return 1;
        TransformationMatrix::Decomposed2Type decomposeData;
        if (!transform.decompose2(decomposeData))
            return 1;
        return std::max(std::abs(decomposeData.scaleX), std::abs(decomposeData.scaleY));
    };

    float rootRelativeScaleFactor = hasNonIdentityTransform() ? computeMaxScaleFromTransform(transform()) : 1;
    if (m_parent) {
        if (m_parent->hasNonIdentityChildrenTransform())
            rootRelativeScaleFactor *= computeMaxScaleFromTransform(m_parent->childrenTransform());
        rootRelativeScaleFactor *= downcast<GraphicsLayerCoordinated>(*m_parent).rootRelativeScaleFactor();
    }

    if (rootRelativeScaleFactor != m_rootRelativeScaleFactor) {
        m_rootRelativeScaleFactor = rootRelativeScaleFactor;
        noteLayerPropertyChanged(Change::ContentsScale, ScheduleFlush::Yes);
    }
}

bool GraphicsLayerCoordinated::setFilters(const FilterOperations& filters)
{
    bool canCompositeFilters = filtersCanBeComposited(filters);
    if (m_filters == filters)
        return canCompositeFilters;

    if (canCompositeFilters)
        GraphicsLayer::setFilters(filters);
    else
        clearFilters();

    noteLayerPropertyChanged(Change::Filters, ScheduleFlush::Yes);
    return canCompositeFilters;
}

void GraphicsLayerCoordinated::setMaskLayer(RefPtr<GraphicsLayer>&& maskLayer)
{
    if (m_maskLayer == maskLayer)
        return;

    GraphicsLayer::setMaskLayer(WTFMove(maskLayer));
    if (m_maskLayer) {
        m_maskLayer->setSize(m_size);
        m_maskLayer->setContentsVisible(m_contentsVisible);
    }
    noteLayerPropertyChanged(Change::Mask, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setReplicatedByLayer(RefPtr<GraphicsLayer>&& replicaLayer)
{
    if (m_replicaLayer == replicaLayer)
        return;

    GraphicsLayer::setReplicatedByLayer(WTFMove(replicaLayer));
    noteLayerPropertyChanged(Change::Replica, ScheduleFlush::Yes);
}

bool GraphicsLayerCoordinated::setBackdropFilters(const FilterOperations& backdropFilters)
{
    bool canCompositeFilters = filtersCanBeComposited(backdropFilters);
    if (m_backdropFilters == backdropFilters)
        return canCompositeFilters;

    if (canCompositeFilters)
        GraphicsLayer::setBackdropFilters(backdropFilters);
    else
        clearBackdropFilters();
    noteLayerPropertyChanged({ Change::Backdrop, Change::DebugIndicators }, ScheduleFlush::Yes);
    return canCompositeFilters;
}

void GraphicsLayerCoordinated::setBackdropFiltersRect(const FloatRoundedRect& backdropFiltersRect)
{
    if (m_backdropFiltersRect == backdropFiltersRect)
        return;

    GraphicsLayer::setBackdropFiltersRect(backdropFiltersRect);
    noteLayerPropertyChanged(Change::BackdropRect, ScheduleFlush::Yes);
}

bool GraphicsLayerCoordinated::addAnimation(const KeyframeValueList& valueList, const GraphicsLayerAnimation* animation, const String& animationName, double timeOffset)
{
    ASSERT(!animationName.isEmpty());
    ASSERT(animation);

    if (animation->isZeroDuration() || valueList.size() < 2)
        return false;

    if (animation->playbackRate() != 1 || !animation->directionIsForwards())
        return false;

    switch (valueList.property()) {
    case AnimatedProperty::WebkitBackdropFilter:
    case AnimatedProperty::Filter: {
        int listIndex = validateFilterOperations(valueList);
        if (listIndex < 0)
            return false;

        const auto& filters = static_cast<const FilterAnimationValue&>(valueList.at(listIndex)).value();
        // The animation of drop-shadow filter with currentColor isn't supported yet.
        // GraphicsLayerCA doesn't accept animations with drap-shadow. Do it here.
        if (filters.hasFilterOfType<FilterOperation::Type::DropShadowWithStyleColor>())
            return false;
        if (!filtersCanBeComposited(filters))
            return false;
        break;
    }
    case AnimatedProperty::Opacity:
    case AnimatedProperty::Translate:
    case AnimatedProperty::Rotate:
    case AnimatedProperty::Scale:
    case AnimatedProperty::Transform:
        break;
    default:
        return false;
    }

    m_animations.add(TextureMapperAnimation(animationName, valueList, *animation, MonotonicTime::now() - Seconds(timeOffset), 0_s, TextureMapperAnimation::State::Playing));
    noteLayerPropertyChanged(Change::Animations, ScheduleFlush::Yes);
    return true;
}

void GraphicsLayerCoordinated::removeAnimation(const String& animationName, std::optional<AnimatedProperty>)
{
    m_animations.remove(animationName);
    noteLayerPropertyChanged(Change::Animations, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::pauseAnimation(const String& animationName, double timeOffset)
{
    m_animations.pause(animationName, Seconds(timeOffset));
    noteLayerPropertyChanged(Change::Animations, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::suspendAnimations(MonotonicTime time)
{
    m_animations.suspend(time);
    noteLayerPropertyChanged(Change::Animations, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::resumeAnimations()
{
    m_animations.resume();
    noteLayerPropertyChanged(Change::Animations, ScheduleFlush::Yes);
}

bool GraphicsLayerCoordinated::isRunningTransformAnimation() const
{
    return m_animations.hasRunningTransformAnimations();
}

void GraphicsLayerCoordinated::transformRelatedPropertyDidChange()
{
    if (isRunningTransformAnimation())
        noteLayerPropertyChanged(Change::Animations, ScheduleFlush::Yes);
}

Vector<GraphicsLayer::AcceleratedAnimationForTesting> GraphicsLayerCoordinated::acceleratedAnimationsForTesting() const
{
    Vector<GraphicsLayer::AcceleratedAnimationForTesting> animations;
    for (auto& animation : m_animations.animations())
        animations.append({ animatedPropertyIDAsString(animation.keyframes().property()), animation.state() == TextureMapperAnimation::State::Playing ? 1.0 : 0.0, false });
    return animations;
}

void GraphicsLayerCoordinated::setShowDebugBorder(bool showDebugBorder)
{
    if (m_showDebugBorder == showDebugBorder)
        return;

    GraphicsLayer::setShowDebugBorder(showDebugBorder);
    noteLayerPropertyChanged(Change::DebugIndicators, ScheduleFlush::Yes);
}

void GraphicsLayerCoordinated::setShowRepaintCounter(bool showRepaintCounter)
{
    if (m_showRepaintCounter == showRepaintCounter)
        return;

    GraphicsLayer::setShowRepaintCounter(showRepaintCounter);
    noteLayerPropertyChanged(Change::DebugIndicators, ScheduleFlush::Yes);
}

static void dumpInnerLayer(TextStream& textStream, const String& label, CoordinatedPlatformLayer* layer, OptionSet<LayerTreeAsTextOptions> options)
{
    if (!layer)
        return;

    Locker locker { layer->lock() };
    textStream << indent << "(" << label << " ";
    if (options & LayerTreeAsTextOptions::Debug)
        textStream << " " << static_cast<void*>(layer);
    textStream << layer->boundsOrigin().x() << ", " << layer->boundsOrigin().y() << " " << layer->size().width() << " x " << layer->size().height();
    if (!layer->contentsVisible())
        textStream << " hidden";
    textStream << ")\n";
}

void GraphicsLayerCoordinated::dumpAdditionalProperties(TextStream& textStream, OptionSet<LayerTreeAsTextOptions> options) const
{
    if (options & LayerTreeAsTextOptions::IncludeContentLayers)
        dumpInnerLayer(textStream, "backdrop layer"_s, m_backdropLayer.get(), options);
}

bool GraphicsLayerCoordinated::filtersCanBeComposited(const FilterOperations& filters) const
{
    return filters.size() && !filters.hasReferenceFilter();
}

void GraphicsLayerCoordinated::noteLayerPropertyChanged(OptionSet<Change> change, ScheduleFlush scheduleFlush)
{
    if (beingDestroyed())
        return;

    bool needsFlush = m_pendingChanges.isEmpty() && !client().isFlushingLayers();
    m_pendingChanges.add(change);
    for (auto* ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        auto& layer = downcast<GraphicsLayerCoordinated>(*ancestor);
        if (layer.m_hasDescendantsWithPendingChanges)
            break;
        layer.m_hasDescendantsWithPendingChanges = true;
    }
    if (scheduleFlush == ScheduleFlush::Yes && needsFlush)
        client().notifyFlushRequired(this);
}

void GraphicsLayerCoordinated::setNeedsUpdateLayerTransform()
{
    m_needsUpdateLayerTransform = true;

    if (auto maskLayer = downcast<GraphicsLayerCoordinated>(m_maskLayer))
        maskLayer->setNeedsUpdateLayerTransform();
    if (auto replicaLayer = downcast<GraphicsLayerCoordinated>(m_replicaLayer))
        replicaLayer->setNeedsUpdateLayerTransform();

    for (auto& child : m_children)
        downcast<GraphicsLayerCoordinated>(child)->setNeedsUpdateLayerTransform();
}

void GraphicsLayerCoordinated::flushCompositingState(const FloatRect& visibleRect)
{
    CommitState commitState;
    commitState.visibleRect = visibleRect;
    if (m_visibleRect != visibleRect) {
        m_visibleRect = visibleRect;
        commitState.ancestorHadChanges = true;
    }

    recursiveCommitChanges(commitState);

    bool hasPendingTilesCreation = updateBackingStoresIfNeeded();
    if (hasPendingTilesCreation)
        client().notifySubsequentFlushRequired(this);
}

void GraphicsLayerCoordinated::flushCompositingStateForThisLayerOnly()
{
    CommitState commitState;
    auto [positionRelativeToBase, pageScaleFactor] = computePositionRelativeToBase();
    commitLayerChanges(commitState, pageScaleFactor, positionRelativeToBase, false);
}

std::pair<FloatPoint, float> GraphicsLayerCoordinated::computePositionRelativeToBase() const
{
    FloatPoint offset;
    float pageScale = 1;
    bool didFindAnyLayerThatAppliesPageScale = false;
    for (const GraphicsLayer* layer = this; layer; layer = layer->parent()) {
        if (layer->appliesPageScale()) {
            pageScale *= layer->pageScaleFactor();
            didFindAnyLayerThatAppliesPageScale = true;
        }

        offset += layer->position();
    }

    if (didFindAnyLayerThatAppliesPageScale)
        return { WTFMove(offset), pageScale };

    return { FloatPoint(), 1 };
}

void GraphicsLayerCoordinated::computePixelAlignmentIfNeeded(float pageScaleFactor, const FloatPoint& positionRelativeToBase, FloatPoint& adjustedPosition, FloatPoint& adjustedBoundsOrigin, FloatPoint3D& adjustedAnchorPoint, FloatSize& adjustedSize)
{
    if (!client().needsPixelAligment() || WTF::isIntegral(pageScaleFactor) || !m_drawsContent || m_masksToBounds) {
        adjustedPosition = m_position;
        adjustedBoundsOrigin = m_boundsOrigin;
        adjustedAnchorPoint = m_anchorPoint;
        adjustedSize = m_size;
        return;
    }

    FloatRect baseRelativeBounds(positionRelativeToBase, m_size);
    FloatRect scaledBounds = baseRelativeBounds;
    float contentsScale = pageScaleFactor * deviceScaleFactor();
    // Scale by the page scale factor to compute the screen-relative bounds.
    scaledBounds.scale(contentsScale);
    // Round to integer boundaries.
    FloatRect alignedBounds = encloseRectToDevicePixels(LayoutRect(scaledBounds), deviceScaleFactor());

    // Convert back to layer coordinates.
    alignedBounds.scale(1 / contentsScale);
    adjustedSize = alignedBounds.size();

    m_pixelAlignmentOffset = baseRelativeBounds.location() - alignedBounds.location();
    adjustedPosition = m_position - m_pixelAlignmentOffset;
    adjustedBoundsOrigin = m_boundsOrigin - m_pixelAlignmentOffset;

    // Now we have to compute a new anchor point which compensates for rounding.
    float anchorPointX = m_anchorPoint.x();
    float anchorPointY = m_anchorPoint.y();
    if (alignedBounds.width())
        anchorPointX = (baseRelativeBounds.width() * anchorPointX + m_pixelAlignmentOffset.width()) / alignedBounds.width();
    if (alignedBounds.height())
        anchorPointY = (baseRelativeBounds.height() * anchorPointY + m_pixelAlignmentOffset.height()) / alignedBounds.height();
    adjustedAnchorPoint = FloatPoint3D(anchorPointX, anchorPointY, m_anchorPoint.z() * contentsScale);
}

void GraphicsLayerCoordinated::updateGeometry(float pageScaleFactor, const FloatPoint& positionRelativeToBase)
{
    FloatPoint adjustedPosition;
    FloatPoint adjustedBoundsOrigin;
    FloatPoint3D adjustedAnchorPoint;
    FloatSize adjustedSize;
    computePixelAlignmentIfNeeded(pageScaleFactor, positionRelativeToBase, adjustedPosition, adjustedBoundsOrigin, adjustedAnchorPoint, adjustedSize);

    m_platformLayer->setPosition(WTFMove(adjustedPosition));
    m_platformLayer->setBoundsOrigin(WTFMove(adjustedBoundsOrigin));
    m_platformLayer->setAnchorPoint(WTFMove(adjustedAnchorPoint));
    m_platformLayer->setSize(WTFMove(adjustedSize));
}

void GraphicsLayerCoordinated::computeLayerTransformIfNeeded(bool affectedByTransformAnimation)
{
    if (!m_needsUpdateLayerTransform && !affectedByTransformAnimation)
        return;

    m_needsUpdateLayerTransform = false;
    TransformationMatrix currentTransform = transform();
    TransformationMatrix futureTransform = currentTransform;

    if (affectedByTransformAnimation) {
        client().getCurrentTransform(this, currentTransform);
        TextureMapperAnimation::ApplicationResult futureApplicationResults;
        m_animations.apply(futureApplicationResults, MonotonicTime::now() + 50_ms, TextureMapperAnimation::KeepInternalState::Yes);
        futureTransform = futureApplicationResults.transform.value_or(currentTransform);
    }

    m_layerTransform.current.setLocalTransform(currentTransform);

    m_layerTransform.current.setAnchorPoint(m_anchorPoint);
    m_layerTransform.current.setPosition(FloatPoint(m_position - m_boundsOrigin));
    m_layerTransform.current.setSize(m_size);

    m_layerTransform.current.setFlattening(!m_preserves3D);
    m_layerTransform.current.setChildrenTransform(childrenTransform());
    m_layerTransform.current.combineTransforms(parent() ? downcast<GraphicsLayerCoordinated>(*parent()).m_layerTransform.current.combinedForChildren() : TransformationMatrix());

    m_layerTransform.cachedCombined = m_layerTransform.current.combined();
    m_layerTransform.cachedInverse = m_layerTransform.cachedCombined.inverse().value_or(TransformationMatrix());

    m_layerTransform.future = m_layerTransform.current;
    m_layerTransform.cachedFutureInverse = m_layerTransform.cachedInverse;

    auto* parentLayer = downcast<GraphicsLayerCoordinated>(parent());
    if (currentTransform != futureTransform || (parentLayer && parentLayer->m_layerTransform.current.combinedForChildren() != parentLayer->m_layerTransform.future.combinedForChildren())) {
        m_layerTransform.future.setLocalTransform(futureTransform);
        m_layerTransform.future.combineTransforms(parentLayer ? parentLayer->m_layerTransform.future.combinedForChildren() : TransformationMatrix());
        m_layerTransform.cachedFutureInverse = m_layerTransform.future.combined().inverse().value_or(TransformationMatrix());
    }

    m_platformLayer->didUpdateLayerTransform();
}

void GraphicsLayerCoordinated::clampToSizeIfRectIsInfinite(FloatRect& rect, const FloatSize& contentsSize)
{
    if (rect.width() >= LayoutUnit::nearlyMax() || rect.width() <= LayoutUnit::nearlyMin()) {
        rect.setX(0);
        rect.setWidth(contentsSize.width());
    }

    if (rect.height() >= LayoutUnit::nearlyMax() || rect.height() <= LayoutUnit::nearlyMin()) {
        rect.setY(0);
        rect.setHeight(contentsSize.height());
    }
}

void GraphicsLayerCoordinated::updateVisibleRect(const FloatRect& rect)
{
    m_platformLayer->setVisibleRect(rect);

    IntRect visibleRect;
    IntRect visibleRectFuture;
    // Non-invertible layers are not visible.
    if (!m_layerTransform.current.combined().isInvertible()) {
        m_platformLayer->setTransformedVisibleRect(WTFMove(visibleRect), WTFMove(visibleRect));
        return;
    }

    // Return a projection of the rect (surface coordinates) onto the layer's plane (layer coordinates).
    // The resulting quad might be squewed and the result is the bounding box of this quad,
    // so it might spread further than the real visible area (and then even more amplified by the cover rect multiplier).
    ASSERT(m_layerTransform.cachedInverse == m_layerTransform.current.combined().inverse().value_or(TransformationMatrix()));
    auto transformedRect = [&](const TransformationMatrix& matrix) -> IntRect {
        FloatRect result = matrix.clampedBoundsOfProjectedQuad(FloatQuad(rect));
        clampToSizeIfRectIsInfinite(result, m_size);
        return enclosingIntRect(result);
    };
    visibleRect = transformedRect(m_layerTransform.cachedInverse);
    visibleRectFuture = visibleRect;
    if (m_layerTransform.cachedInverse != m_layerTransform.cachedFutureInverse)
        visibleRectFuture.unite(transformedRect(m_layerTransform.cachedFutureInverse));
    m_platformLayer->setTransformedVisibleRect(WTFMove(visibleRect), WTFMove(visibleRectFuture));
}

void GraphicsLayerCoordinated::updateBackdropFilters()
{
    bool canHaveBackdropFilters = needsBackdrop();
    if (!canHaveBackdropFilters) {
        m_platformLayer->setBackdrop(nullptr);
        if (m_backdropLayer) {
            m_backdropLayer->setOwner(nullptr);
            m_backdropLayer = nullptr;
        }
        return;
    }

    if (!m_pendingChanges.contains(Change::Backdrop) && m_backdropLayer)
        return;

    bool isNewLayer = !m_backdropLayer;
    if (isNewLayer) {
        m_backdropLayer = CoordinatedPlatformLayer::create(m_platformLayer->client());
        m_backdropLayer->setOwner(this);
    }

    {
        Locker locker { m_backdropLayer->lock() };
        if (isNewLayer) {
            m_backdropLayer->setAnchorPoint(FloatPoint3D());
            m_backdropLayer->setMasksToBounds(true);
        }

        m_backdropLayer->setContentsVisible(m_contentsVisible);
        m_backdropLayer->setFilters(m_backdropFilters);
    }

    if (isNewLayer)
        updateBackdropFiltersRect();

    m_platformLayer->setBackdrop(m_backdropLayer.get());
}

void GraphicsLayerCoordinated::updateBackdropFiltersRect()
{
    if (!m_backdropLayer)
        return;

    {
        Locker locker { m_backdropLayer->lock() };
        m_backdropLayer->setPosition(m_backdropFiltersRect.rect().location());
        m_backdropLayer->setSize(m_backdropFiltersRect.rect().size());
    }

    m_platformLayer->setBackdropRect(m_backdropFiltersRect);
}

void GraphicsLayerCoordinated::updateAnimations()
{
    m_animations.setTranslate(client().transformMatrixForProperty(AnimatedProperty::Translate));
    m_animations.setRotate(client().transformMatrixForProperty(AnimatedProperty::Rotate));
    m_animations.setScale(client().transformMatrixForProperty(AnimatedProperty::Scale));
    m_animations.setTransform(client().transformMatrixForProperty(AnimatedProperty::Transform));

    m_platformLayer->setAnimations(m_animations);
}

void GraphicsLayerCoordinated::updateIndicators()
{
    Color borderColor;
    float borderWidth = 0;
    if (m_showDebugBorder)
        getDebugBorderInfo(borderColor, borderWidth);

    m_platformLayer->setDebugBorder(WTFMove(borderColor), borderWidth);
    m_platformLayer->setShowRepaintCounter(m_showRepaintCounter);
}

void GraphicsLayerCoordinated::commitLayerChanges(CommitState& commitState, float pageScaleFactor, const FloatPoint& positionRelativeToBase, bool affectedByTransformAnimation)
{
    Locker locker { m_platformLayer->lock() };

    if (m_pendingChanges.contains(Change::ContentsBuffer)) {
        if (!m_contentsDisplayDelegate && !m_contentsBufferProxy)
            m_platformLayer->setContentsBuffer(nullptr);
    }

    bool contentsBufferNeedsDisplay = false;
    if (m_pendingChanges.contains(Change::ContentsBufferNeedsDisplay)) {
        if (m_contentsDisplayDelegate) {
            if (!m_contentsDisplayDelegate->display(m_platformLayer.get(), std::exchange(m_contentsDirtyRegion, std::nullopt)))
                contentsBufferNeedsDisplay = true;
        } else if (m_contentsBufferProxy)
            m_contentsBufferProxy->consumePendingBufferIfNeeded();
    }

    if (m_pendingChanges.containsAny(Change::Geometry))
        updateGeometry(pageScaleFactor, positionRelativeToBase);

    if (m_pendingChanges.contains(Change::Transform))
        m_platformLayer->setTransform(transform());

    if (m_pendingChanges.contains(Change::ChildrenTransform))
        m_platformLayer->setChildrenTransform(childrenTransform());

    if (m_pendingChanges.contains(Change::Preserves3D))
        m_platformLayer->setPreserves3D(m_preserves3D);

    if (m_pendingChanges.contains(Change::MasksToBounds))
        m_platformLayer->setMasksToBounds(m_masksToBounds);

    computeLayerTransformIfNeeded(affectedByTransformAnimation);
    updateVisibleRect(commitState.visibleRect);

#if ENABLE(SCROLLING_THREAD)
    if (m_pendingChanges.contains(Change::ScrollingNode))
        m_platformLayer->setScrollingNodeID(m_scrollingNodeID);
#endif

    if (m_pendingChanges.contains(Change::DrawsContent))
        m_platformLayer->setDrawsContent(m_drawsContent);

    if (m_pendingChanges.contains(Change::BackfaceVisibility))
        m_platformLayer->setBackfaceVisibility(m_backfaceVisibility);

    if (m_pendingChanges.contains(Change::Opacity))
        m_platformLayer->setOpacity(m_opacity);

    if (m_pendingChanges.contains(Change::ContentsVisible)) {
        m_platformLayer->setContentsVisible(m_contentsVisible);
        if (m_backdropLayer) {
            Locker locker { m_backdropLayer->lock() };
            m_backdropLayer->setContentsVisible(m_contentsVisible);
        }
    }

    if (m_pendingChanges.contains(Change::ContentsOpaque))
        m_platformLayer->setContentsOpaque(m_contentsOpaque);

    if (m_pendingChanges.contains(Change::ContentsRect))
        m_platformLayer->setContentsRect(m_contentsRect);

    if (m_pendingChanges.contains(Change::ContentsRectClipsDescendants))
        m_platformLayer->setContentsRectClipsDescendants(m_contentsRectClipsDescendants);

    if (m_pendingChanges.contains(Change::ContentsTiling)) {
        m_platformLayer->setContentsTileSize(m_contentsTileSize);
        m_platformLayer->setContentsTileSize(m_contentsTilePhase);
    }

    if (m_pendingChanges.contains(Change::ContentsClippingRect))
        m_platformLayer->setContentsClippingRect(m_contentsClippingRect);

    updateRootRelativeScale(); // Needs to happen before Change::ContentsScale.

    if (m_pendingChanges.contains(Change::ContentsScale))
        m_platformLayer->setContentsScale(pageScaleFactor * deviceScaleFactor() * m_rootRelativeScaleFactor);

    if (m_pendingChanges.contains(Change::ContentsImage))
        m_platformLayer->setContentsImage(m_contentsImage.get());

    if (m_pendingChanges.contains(Change::ContentsColor))
        m_platformLayer->setContentsColor(m_contentsColor);

    if (m_pendingChanges.contains(Change::DirtyRegion)) {
        ASSERT(m_dirtyRegion.has_value());
        m_platformLayer->setDirtyRegion(*std::exchange(m_dirtyRegion, std::nullopt));
    }

    if (m_pendingChanges.contains(Change::Filters))
        m_platformLayer->setFilters(m_filters);

    if (m_pendingChanges.contains(Change::Mask))
        m_platformLayer->setMask(m_maskLayer ? &downcast<GraphicsLayerCoordinated>(m_maskLayer)->coordinatedPlatformLayer() : nullptr);

    if (m_pendingChanges.contains(Change::Replica))
        m_platformLayer->setReplica(m_replicaLayer ? &downcast<GraphicsLayerCoordinated>(m_replicaLayer)->coordinatedPlatformLayer() : nullptr);

    if (m_pendingChanges.contains(Change::Backdrop) || needsBackdrop())
        updateBackdropFilters();

    if (m_pendingChanges.contains(Change::BackdropRect))
        updateBackdropFiltersRect();

    if (m_pendingChanges.contains(Change::Animations))
        updateAnimations();

    if (m_pendingChanges.contains(Change::EventRegion))
        m_platformLayer->setEventRegion(m_eventRegion);

    if (m_pendingChanges.contains(Change::DebugIndicators))
        updateIndicators();

    if (m_pendingChanges.contains(Change::Children)) {
        m_platformLayer->setChildren(m_children.map<Vector<Ref<CoordinatedPlatformLayer>>>([](const auto& child) {
            return Ref { downcast<GraphicsLayerCoordinated>(child)->coordinatedPlatformLayer() };
        }));
    }

    m_platformLayer->updateContents(affectedByTransformAnimation);

    m_pendingChanges = { };

    if (contentsBufferNeedsDisplay)
        m_pendingChanges.add(Change::ContentsBufferNeedsDisplay);
}

bool GraphicsLayerCoordinated::needsCommit(CommitState& commitState) const
{
    if (renderingIsSuppressedIncludingDescendants())
        return false;

    if (commitState.ancestorHadChanges)
        return true;

    if (!m_pendingChanges.isEmpty())
        return true;

    if (m_hasDescendantsWithPendingChanges)
        return true;

    if (isRunningTransformAnimation() || m_hasDescendantsWithRunningTransformAnimations)
        return true;

    if (m_platformLayer->hasPendingTilesCreation() || m_hasDescendantsWithPendingTilesCreation)
        return true;

    return false;
}

void GraphicsLayerCoordinated::recursiveCommitChanges(CommitState& commitState, float pageScaleFactor, const FloatPoint& positionRelativeToBase, bool affectedByPageScale)
{
    if (!needsCommit(commitState))
        return;

    CommitState childCommitState = commitState;
    if (!childCommitState.ancestorHadChanges)
        childCommitState.ancestorHadChanges = !m_pendingChanges.isEmpty();

    if (appliesPageScale()) {
        pageScaleFactor *= this->pageScaleFactor();
        affectedByPageScale = true;
    }

    bool runningTransformAnimation = isRunningTransformAnimation();
    bool affectedByTransformAnimation = commitState.ancestorHasTransformAnimation || runningTransformAnimation;

    // Accumulate an offset from the ancestral pixel-aligned layer.
    FloatPoint baseRelativePosition = positionRelativeToBase;
    if (affectedByPageScale)
        baseRelativePosition += m_position;
    commitLayerChanges(childCommitState, pageScaleFactor, baseRelativePosition, affectedByTransformAnimation);

    if (runningTransformAnimation)
        childCommitState.ancestorHasTransformAnimation = true;

    if (auto maskLayer = downcast<GraphicsLayerCoordinated>(m_maskLayer))
        maskLayer->commitLayerChanges(childCommitState, pageScaleFactor, baseRelativePosition, affectedByTransformAnimation);
    if (auto replicaLayer = downcast<GraphicsLayerCoordinated>(m_replicaLayer))
        replicaLayer->commitLayerChanges(childCommitState, pageScaleFactor, baseRelativePosition, affectedByTransformAnimation);

    bool hasDescendantsWithPendingTilesCreation = false;
    bool hasDescendantsWithRunningTransformAnimations = false;
    for (auto& currentChild : children()) {
        auto& child = downcast<GraphicsLayerCoordinated>(currentChild.get());
        child.recursiveCommitChanges(childCommitState, pageScaleFactor, positionRelativeToBase, affectedByPageScale);

        if (child.coordinatedPlatformLayer().hasPendingTilesCreation() || child.m_hasDescendantsWithPendingTilesCreation)
            hasDescendantsWithPendingTilesCreation = true;
        if (child.isRunningTransformAnimation() || child.m_hasDescendantsWithRunningTransformAnimations)
            hasDescendantsWithRunningTransformAnimations = true;
    }

    m_hasDescendantsWithPendingChanges = false;
    m_hasDescendantsWithPendingTilesCreation = hasDescendantsWithPendingTilesCreation;
    m_hasDescendantsWithRunningTransformAnimations = hasDescendantsWithRunningTransformAnimations;
}

bool GraphicsLayerCoordinated::updateBackingStoresIfNeeded()
{
    bool hasPendingTilesCreation = false;
    if (auto maskLayer = downcast<GraphicsLayerCoordinated>(m_maskLayer))
        hasPendingTilesCreation |= maskLayer->updateBackingStoreIfNeeded();
    if (auto replicaLayer = downcast<GraphicsLayerCoordinated>(m_replicaLayer))
        hasPendingTilesCreation |= replicaLayer->updateBackingStoreIfNeeded();

    hasPendingTilesCreation |= updateBackingStoreIfNeeded();

    for (auto& currentChild : children()) {
        auto& child = downcast<GraphicsLayerCoordinated>(currentChild.get());
        hasPendingTilesCreation |= child.updateBackingStoresIfNeeded();
    }

    return hasPendingTilesCreation;
}

bool GraphicsLayerCoordinated::updateBackingStoreIfNeeded()
{
    m_platformLayer->updateBackingStore();
    return m_platformLayer->hasPendingTilesCreation();
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
