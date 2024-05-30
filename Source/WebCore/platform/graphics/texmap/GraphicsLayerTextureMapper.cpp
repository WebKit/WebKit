/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2022 Sony Interactive Entertainment Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "GraphicsLayerTextureMapper.h"

#include "GraphicsContext.h"
#include "GraphicsLayerFactory.h"
#include "ImageBuffer.h"
#include "NicosiaAnimation.h"
#include "TransformOperation.h"

#if !USE(COORDINATED_GRAPHICS)

namespace WebCore {

Ref<GraphicsLayer> GraphicsLayer::create(GraphicsLayerFactory* factory, GraphicsLayerClient& client, Type layerType)
{
    if (!factory)
        return adoptRef(*new GraphicsLayerTextureMapper(layerType, client));

    return factory->createGraphicsLayer(layerType, client);
}

GraphicsLayerTextureMapper::GraphicsLayerTextureMapper(Type layerType, GraphicsLayerClient& client)
    : GraphicsLayer(layerType, client)
    , m_changeMask(NoChanges)
    , m_needsDisplay(false)
    , m_debugBorderWidth(0)
    , m_contentsLayer(0)
{
}

void GraphicsLayerTextureMapper::notifyChange(ChangeMask changeMask)
{
    bool flushRequired = m_changeMask == NoChanges;
    m_changeMask |= changeMask;

    if (flushRequired)
        client().notifyFlushRequired(this);
}

GraphicsLayerTextureMapper::~GraphicsLayerTextureMapper()
{
    if (m_contentsLayer)
        m_contentsLayer->setClient(0);

    willBeDestroyed();
}

void GraphicsLayerTextureMapper::setNeedsDisplay()
{
    if (!drawsContent())
        return;

    // The current size might change, thus we need to update the whole display.
    m_needsDisplay = true;
    notifyChange(DisplayChange);
    addRepaintRect(FloatRect(FloatPoint(), m_size));
}

void GraphicsLayerTextureMapper::setContentsNeedsDisplay()
{
    notifyChange(DisplayChange);
    addRepaintRect(contentsRect());
}

void GraphicsLayerTextureMapper::setNeedsDisplayInRect(const FloatRect& rect, ShouldClipToLayer)
{
    if (!drawsContent())
        return;

    if (m_needsDisplay)
        return;
    m_needsDisplayRect.unite(rect);
    notifyChange(DisplayChange);
    addRepaintRect(rect);
}

bool GraphicsLayerTextureMapper::setChildren(Vector<Ref<GraphicsLayer>>&& children)
{
    if (GraphicsLayer::setChildren(WTFMove(children))) {
        notifyChange(ChildrenChange);
        return true;
    }
    return false;
}

void GraphicsLayerTextureMapper::addChild(Ref<GraphicsLayer>&& layer)
{
    notifyChange(ChildrenChange);
    GraphicsLayer::addChild(WTFMove(layer));
}

void GraphicsLayerTextureMapper::addChildAtIndex(Ref<GraphicsLayer>&& layer, int index)
{
    GraphicsLayer::addChildAtIndex(WTFMove(layer), index);
    notifyChange(ChildrenChange);
}

void GraphicsLayerTextureMapper::addChildAbove(Ref<GraphicsLayer>&& layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildAbove(WTFMove(layer), sibling);
    notifyChange(ChildrenChange);
}

void GraphicsLayerTextureMapper::addChildBelow(Ref<GraphicsLayer>&& layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(WTFMove(layer), sibling);
    notifyChange(ChildrenChange);
}

bool GraphicsLayerTextureMapper::replaceChild(GraphicsLayer* oldChild, Ref<GraphicsLayer>&& newChild)
{
    if (GraphicsLayer::replaceChild(oldChild, WTFMove(newChild))) {
        notifyChange(ChildrenChange);
        return true;
    }
    return false;
}

void GraphicsLayerTextureMapper::setMaskLayer(RefPtr<GraphicsLayer>&& value)
{
    if (value == maskLayer())
        return;

    GraphicsLayer* rawLayer = value.get();
    GraphicsLayer::setMaskLayer(WTFMove(value));
    notifyChange(MaskLayerChange);

    if (!rawLayer)
        return;
    rawLayer->setSize(size());
    rawLayer->setContentsVisible(contentsAreVisible());
}


void GraphicsLayerTextureMapper::setReplicatedByLayer(RefPtr<GraphicsLayer>&& value)
{
    if (value == replicaLayer())
        return;
    GraphicsLayer::setReplicatedByLayer(WTFMove(value));
    notifyChange(ReplicaLayerChange);
}

void GraphicsLayerTextureMapper::setPosition(const FloatPoint& value)
{
    if (value == position())
        return;
    GraphicsLayer::setPosition(value);
    notifyChange(PositionChange);
}

void GraphicsLayerTextureMapper::setAnchorPoint(const FloatPoint3D& value)
{
    if (value == anchorPoint())
        return;
    GraphicsLayer::setAnchorPoint(value);
    notifyChange(AnchorPointChange);
}

void GraphicsLayerTextureMapper::setSize(const FloatSize& value)
{
    if (value == size())
        return;

    GraphicsLayer::setSize(value);
    if (maskLayer())
        maskLayer()->setSize(value);
    notifyChange(SizeChange);
}

void GraphicsLayerTextureMapper::setTransform(const TransformationMatrix& value)
{
    if (value == transform())
        return;

    GraphicsLayer::setTransform(value);
    notifyChange(TransformChange);
}

void GraphicsLayerTextureMapper::setChildrenTransform(const TransformationMatrix& value)
{
    if (value == childrenTransform())
        return;
    GraphicsLayer::setChildrenTransform(value);
    notifyChange(ChildrenTransformChange);
}

void GraphicsLayerTextureMapper::setPreserves3D(bool value)
{
    if (value == preserves3D())
        return;
    GraphicsLayer::setPreserves3D(value);
    notifyChange(Preserves3DChange);
}

void GraphicsLayerTextureMapper::setMasksToBounds(bool value)
{
    if (value == masksToBounds())
        return;
    GraphicsLayer::setMasksToBounds(value);
    notifyChange(MasksToBoundsChange);
}

void GraphicsLayerTextureMapper::setDrawsContent(bool value)
{
    if (value == drawsContent())
        return;
    GraphicsLayer::setDrawsContent(value);
    notifyChange(DrawsContentChange);

    if (value)
        setNeedsDisplay();
}

void GraphicsLayerTextureMapper::setContentsVisible(bool value)
{
    if (value == contentsAreVisible())
        return;
    notifyChange(ContentsVisibleChange);
    GraphicsLayer::setContentsVisible(value);
    if (maskLayer())
        maskLayer()->setContentsVisible(value);
}

void GraphicsLayerTextureMapper::setContentsOpaque(bool value)
{
    if (value == contentsOpaque())
        return;
    notifyChange(ContentsOpaqueChange);
    GraphicsLayer::setContentsOpaque(value);
}

void GraphicsLayerTextureMapper::setBackfaceVisibility(bool value)
{
    if (value == backfaceVisibility())
        return;
    GraphicsLayer::setBackfaceVisibility(value);
    notifyChange(BackfaceVisibilityChange);
}

void GraphicsLayerTextureMapper::setBackgroundColor(const Color& value)
{
    if (value == backgroundColor())
        return;
    GraphicsLayer::setBackgroundColor(value);
    notifyChange(BackgroundColorChange);
}

void GraphicsLayerTextureMapper::setOpacity(float value)
{
    if (value == opacity())
        return;
    GraphicsLayer::setOpacity(value);
    notifyChange(OpacityChange);
}

void GraphicsLayerTextureMapper::setContentsRect(const FloatRect& value)
{
    if (value == contentsRect())
        return;
    GraphicsLayer::setContentsRect(value);
    notifyChange(ContentsRectChange);
}

void GraphicsLayerTextureMapper::setContentsClippingRect(const FloatRoundedRect& rect)
{
    if (rect == m_contentsClippingRect)
        return;

    GraphicsLayer::setContentsClippingRect(rect);
    notifyChange(ContentsRectChange);
}

void GraphicsLayerTextureMapper::setContentsRectClipsDescendants(bool contentsRectClipsDescendants)
{
    if (contentsRectClipsDescendants == m_contentsRectClipsDescendants)
        return;

    GraphicsLayer::setContentsRectClipsDescendants(contentsRectClipsDescendants);
    notifyChange(ContentsRectChange);
}

void GraphicsLayerTextureMapper::setContentsToSolidColor(const Color& color)
{
    if (color == m_solidColor)
        return;

    m_solidColor = color;
    notifyChange(SolidColorChange);
}

void GraphicsLayerTextureMapper::setContentsToImage(Image* image)
{
    if (image) {
        // Make the decision about whether the image has changed.
        // This code makes the assumption that pointer equality on a PlatformImagePtr is a valid way to tell if the image is changed.
        // This assumption is true for the GTK+ port.
        auto newNativeImage = image->currentNativeImage();
        if (!newNativeImage)
            return;

        if (newNativeImage == m_compositedNativeImage)
            return;

        m_compositedNativeImage = newNativeImage;
        if (!m_compositedImage)
            m_compositedImage = TextureMapperTiledBackingStore::create();
        m_compositedImage->setContentsToImage(image);
        m_compositedImage->updateContentsScale(pageScaleFactor() * deviceScaleFactor());
    } else {
        m_compositedNativeImage = nullptr;
        m_compositedImage = nullptr;
    }

    setContentsToPlatformLayer(m_compositedImage.get(), ContentsLayerPurpose::Image);
    notifyChange(ContentChange);
    GraphicsLayer::setContentsToImage(image);
}

void GraphicsLayerTextureMapper::setContentsToPlatformLayer(TextureMapperPlatformLayer* platformLayer, ContentsLayerPurpose purpose)
{
    if (platformLayer == m_contentsLayer)
        return;

    GraphicsLayer::setContentsToPlatformLayer(platformLayer, purpose);
    notifyChange(ContentChange);

    if (m_contentsLayer)
        m_contentsLayer->setClient(0);

    m_contentsLayer = platformLayer;

    if (m_contentsLayer)
        m_contentsLayer->setClient(this);
}

void GraphicsLayerTextureMapper::setContentsDisplayDelegate(RefPtr<GraphicsLayerContentsDisplayDelegate>&& displayDelegate, ContentsLayerPurpose purpose)
{
    PlatformLayer* platformLayer = displayDelegate ? displayDelegate->platformLayer() : nullptr;
    setContentsToPlatformLayer(platformLayer, purpose);
}

void GraphicsLayerTextureMapper::setShowDebugBorder(bool show)
{
    if (isShowingDebugBorder() == show)
        return;

    GraphicsLayer::setShowDebugBorder(show);
    notifyChange(DebugVisualsChange);
}

void GraphicsLayerTextureMapper::setShowRepaintCounter(bool show)
{
    if (isShowingRepaintCounter() == show)
        return;

    GraphicsLayer::setShowRepaintCounter(show);
    notifyChange(RepaintCountChange);
}

void GraphicsLayerTextureMapper::flushCompositingStateForThisLayerOnly()
{
    prepareBackingStoreIfNeeded();
    commitLayerChanges();
    m_layer.syncAnimations(MonotonicTime::now());
}

void GraphicsLayerTextureMapper::prepareBackingStoreIfNeeded()
{
    if (shouldHaveBackingStore()) {
        if (!m_backingStore) {
            m_backingStore = TextureMapperTiledBackingStore::create();
            m_changeMask |= BackingStoreChange;
        }
    } else {
        if (m_backingStore) {
            m_backingStore = nullptr;
            m_changeMask |= BackingStoreChange;
        }
    }

    updateDebugBorderAndRepaintCount();
}

void GraphicsLayerTextureMapper::updateDebugBorderAndRepaintCount()
{
    if (isShowingDebugBorder())
        updateDebugIndicators();

    // When this has its own backing store (e.g. Qt WK1), update the repaint count before calling TextureMapperLayer::flushCompositingStateForThisLayerOnly().
    bool needsToRepaint = shouldHaveBackingStore() && (m_needsDisplay || !m_needsDisplayRect.isEmpty());
    if (isShowingRepaintCounter() && needsToRepaint) {
        incrementRepaintCount();
        m_changeMask |= RepaintCountChange;
    }
}

void GraphicsLayerTextureMapper::setDebugBorder(const Color& color, float width)
{
    m_debugBorderColor = color;
    m_debugBorderWidth = width;
    m_changeMask |= DebugVisualsChange;
}

void GraphicsLayerTextureMapper::commitLayerChanges()
{
    if (m_changeMask == NoChanges)
        return;

    if (m_changeMask & ChildrenChange) {
        auto rawChildren = WTF::map(children(), [](auto& child) -> TextureMapperLayer* {
            return &downcast<GraphicsLayerTextureMapper>(child.get()).layer();
        });
        m_layer.setChildren(WTFMove(rawChildren));
    }

    if (m_changeMask & MaskLayerChange) {
        auto* layer = downcast<GraphicsLayerTextureMapper>(maskLayer());
        m_layer.setMaskLayer(layer ? &layer->layer() : nullptr);
    }

    if (m_changeMask & ReplicaLayerChange) {
        auto* layer = downcast<GraphicsLayerTextureMapper>(replicaLayer());
        m_layer.setReplicaLayer(layer ? &layer->layer() : nullptr);
    }

    if (m_changeMask & BackdropLayerChange) {
        if (needsBackdrop()) {
            if (!m_backdropLayer) {
                m_backdropLayer = makeUnique<TextureMapperLayer>();
                m_backdropLayer->setAnchorPoint(FloatPoint3D());
                m_backdropLayer->setContentsVisible(true);
                m_backdropLayer->setMasksToBounds(true);
            }
            m_backdropLayer->setFilters(m_backdropFilters);
            m_backdropLayer->setSize(m_backdropFiltersRect.rect().size());
            m_backdropLayer->setPosition(m_backdropFiltersRect.rect().location());
        } else
            m_backdropLayer = nullptr;

        m_layer.setBackdropLayer(m_backdropLayer.get());
        m_layer.setBackdropFiltersRect(m_backdropFiltersRect);
    }

    if (m_changeMask & PositionChange)
        m_layer.setPosition(position());

    if (m_changeMask & AnchorPointChange)
        m_layer.setAnchorPoint(anchorPoint());

    if (m_changeMask & SizeChange)
        m_layer.setSize(size());

    if (m_changeMask & TransformChange)
        m_layer.setTransform(transform());

    if (m_changeMask & ChildrenTransformChange)
        m_layer.setChildrenTransform(childrenTransform());

    if (m_changeMask & Preserves3DChange)
        m_layer.setPreserves3D(preserves3D());

    if (m_changeMask & ContentsRectChange) {
        m_layer.setContentsRect(contentsRect());
        m_layer.setContentsClippingRect(contentsClippingRect());
        m_layer.setContentsRectClipsDescendants(contentsRectClipsDescendants());
    }

    if (m_changeMask & MasksToBoundsChange)
        m_layer.setMasksToBounds(masksToBounds());

    if (m_changeMask & DrawsContentChange)
        m_layer.setDrawsContent(drawsContent());

    if (m_changeMask & ContentsVisibleChange)
        m_layer.setContentsVisible(contentsAreVisible());

    if (m_changeMask & ContentsOpaqueChange)
        m_layer.setContentsOpaque(contentsOpaque());

    if (m_changeMask & BackfaceVisibilityChange)
        m_layer.setBackfaceVisibility(backfaceVisibility());

    if (m_changeMask & BackgroundColorChange)
        m_layer.setBackgroundColor(backgroundColor());

    if (m_changeMask & OpacityChange)
        m_layer.setOpacity(opacity());

    if (m_changeMask & SolidColorChange)
        m_layer.setSolidColor(m_solidColor);

    if (m_changeMask & FilterChange)
        m_layer.setFilters(filters());

    if (m_changeMask & BackingStoreChange)
        m_layer.setBackingStore(m_backingStore.get());

    if (m_changeMask & DebugVisualsChange) {
        m_layer.setShowDebugBorder(isShowingDebugBorder());
        m_layer.setDebugBorderColor(debugBorderColor());
        m_layer.setDebugBorderWidth(debugBorderWidth());
    }

    if (m_changeMask & RepaintCountChange) {
        m_layer.setShowRepaintCounter(isShowingRepaintCounter());
        m_layer.setRepaintCount(repaintCount());
    }

    if (m_changeMask & ContentChange)
        m_layer.setContentsLayer(platformLayer());

    if (m_changeMask & AnimationChange)
        m_layer.setAnimations(m_animations);

    if (m_changeMask & AnimationStarted)
        client().notifyAnimationStarted(this, emptyString(), m_animationStartTime);

    m_changeMask = NoChanges;
}

void GraphicsLayerTextureMapper::flushCompositingState(const FloatRect& rect)
{
    flushCompositingStateForThisLayerOnly();

    auto now = MonotonicTime::now();

    if (maskLayer())
        maskLayer()->flushCompositingState(rect);
    if (replicaLayer()) {
        replicaLayer()->flushCompositingState(rect);
        downcast<GraphicsLayerTextureMapper>(replicaLayer())->layer().applyAnimationsRecursively(now);
    }
    if (m_backdropLayer)
        m_backdropLayer->applyAnimationsRecursively(now);
    for (auto& child : children())
        child->flushCompositingState(rect);
}

void GraphicsLayerTextureMapper::updateBackingStoreIncludingSubLayers(TextureMapper& textureMapper)
{
    updateBackingStoreIfNeeded(textureMapper);

    if (maskLayer())
        downcast<GraphicsLayerTextureMapper>(*maskLayer()).updateBackingStoreIfNeeded(textureMapper);
    if (replicaLayer())
        downcast<GraphicsLayerTextureMapper>(*replicaLayer()).updateBackingStoreIncludingSubLayers(textureMapper);
    for (auto& child : children())
        downcast<GraphicsLayerTextureMapper>(child.get()).updateBackingStoreIncludingSubLayers(textureMapper);
}

void GraphicsLayerTextureMapper::updateBackingStoreIfNeeded(TextureMapper& textureMapper)
{
    if (!shouldHaveBackingStore()) {
        ASSERT(!m_backingStore);
        return;
    }
    ASSERT(m_backingStore);

    IntRect dirtyRect = enclosingIntRect(FloatRect(FloatPoint::zero(), m_size));
    if (!m_needsDisplay)
        dirtyRect.intersect(enclosingIntRect(m_needsDisplayRect));
    if (dirtyRect.isEmpty())
        return;

    m_backingStore->updateContentsScale(pageScaleFactor() * deviceScaleFactor());

    dirtyRect.scale(pageScaleFactor() * deviceScaleFactor());
    m_backingStore->updateContents(textureMapper, this, m_size, dirtyRect);

    m_needsDisplay = false;
    m_needsDisplayRect = IntRect();
}

bool GraphicsLayerTextureMapper::shouldHaveBackingStore() const
{
    return drawsContent() && contentsAreVisible() && !m_size.isEmpty();
}

bool GraphicsLayerTextureMapper::filtersCanBeComposited(const FilterOperations& filters) const
{
    if (!filters.size())
        return false;

    return !filters.hasReferenceFilter();
}

bool GraphicsLayerTextureMapper::addAnimation(const KeyframeValueList& valueList, const FloatSize& boxSize, const Animation* anim, const String& keyframesName, double timeOffset)
{
    ASSERT(!keyframesName.isEmpty());

    if (!anim || anim->isEmptyOrZeroDuration() || valueList.size() < 2 || (valueList.property() != AnimatedProperty::Transform && valueList.property() != AnimatedProperty::Opacity))
        return false;

    if (valueList.property() == AnimatedProperty::Filter) {
        int listIndex = validateFilterOperations(valueList);
        if (listIndex < 0)
            return false;

        const auto& filters = static_cast<const FilterAnimationValue&>(valueList.at(listIndex)).value();
        if (!filtersCanBeComposited(filters))
            return false;
    }

    const MonotonicTime currentTime = MonotonicTime::now();
    m_animations.add(Nicosia::Animation(keyframesName, valueList, boxSize, *anim, currentTime - Seconds(timeOffset), 0_s, Nicosia::Animation::AnimationState::Playing));
    // m_animationStartTime is the time of the first real frame of animation, now or delayed by a negative offset.
    if (Seconds(timeOffset) > 0_s)
        m_animationStartTime = currentTime;
    else
        m_animationStartTime = currentTime - Seconds(timeOffset);
    notifyChange(AnimationChange);
    notifyChange(AnimationStarted);
    return true;
}

void GraphicsLayerTextureMapper::pauseAnimation(const String& animationName, double timeOffset)
{
    m_animations.pause(animationName, Seconds(timeOffset));
}

void GraphicsLayerTextureMapper::removeAnimation(const String& animationName, std::optional<AnimatedProperty>)
{
    m_animations.remove(animationName);
}

bool GraphicsLayerTextureMapper::setFilters(const FilterOperations& filters)
{
    bool canCompositeFilters = filtersCanBeComposited(filters);
    if (GraphicsLayer::filters() == filters)
        return canCompositeFilters;

    if (canCompositeFilters) {
        if (!GraphicsLayer::setFilters(filters))
            return false;
        notifyChange(FilterChange);
    } else if (GraphicsLayer::filters().size()) {
        clearFilters();
        notifyChange(FilterChange);
    }

    return canCompositeFilters;
}

bool GraphicsLayerTextureMapper::setBackdropFilters(const FilterOperations& filters)
{
    bool canCompositeFilters = filtersCanBeComposited(filters);
    if (m_backdropFilters == filters)
        return canCompositeFilters;

    if (canCompositeFilters)
        GraphicsLayer::setBackdropFilters(filters);
    else
        clearBackdropFilters();

    notifyChange(BackdropLayerChange);

    return canCompositeFilters;
}

void GraphicsLayerTextureMapper::setBackdropFiltersRect(const FloatRoundedRect& backdropFiltersRect)
{
    if (m_backdropFiltersRect == backdropFiltersRect)
        return;

    GraphicsLayer::setBackdropFiltersRect(backdropFiltersRect);
    notifyChange(BackdropLayerChange);
}

}
#endif
