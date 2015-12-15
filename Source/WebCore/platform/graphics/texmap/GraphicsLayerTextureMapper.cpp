/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

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
#include "TextureMapperAnimation.h"
#include <wtf/CurrentTime.h>

#if USE(TEXTURE_MAPPER) && !USE(COORDINATED_GRAPHICS)

namespace WebCore {

std::unique_ptr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerFactory* factory, GraphicsLayerClient& client, Type layerType)
{
    if (!factory)
        return std::make_unique<GraphicsLayerTextureMapper>(layerType, client);

    return factory->createGraphicsLayer(layerType, client);
}

GraphicsLayerTextureMapper::GraphicsLayerTextureMapper(Type layerType, GraphicsLayerClient& client)
    : GraphicsLayer(layerType, client)
    , m_compositedNativeImagePtr(0)
    , m_changeMask(NoChanges)
    , m_needsDisplay(false)
    , m_fixedToViewport(false)
    , m_debugBorderWidth(0)
    , m_contentsLayer(0)
    , m_animationStartTime(0)
    , m_isScrollable(false)
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

bool GraphicsLayerTextureMapper::setChildren(const Vector<GraphicsLayer*>& children)
{
    if (GraphicsLayer::setChildren(children)) {
        notifyChange(ChildrenChange);
        return true;
    }
    return false;
}

void GraphicsLayerTextureMapper::addChild(GraphicsLayer* layer)
{
    notifyChange(ChildrenChange);
    GraphicsLayer::addChild(layer);
}

void GraphicsLayerTextureMapper::addChildAtIndex(GraphicsLayer* layer, int index)
{
    GraphicsLayer::addChildAtIndex(layer, index);
    notifyChange(ChildrenChange);
}

void GraphicsLayerTextureMapper::addChildAbove(GraphicsLayer* layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildAbove(layer, sibling);
    notifyChange(ChildrenChange);
}

void GraphicsLayerTextureMapper::addChildBelow(GraphicsLayer* layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(layer, sibling);
    notifyChange(ChildrenChange);
}

bool GraphicsLayerTextureMapper::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    if (GraphicsLayer::replaceChild(oldChild, newChild)) {
        notifyChange(ChildrenChange);
        return true;
    }
    return false;
}

void GraphicsLayerTextureMapper::setMaskLayer(GraphicsLayer* value)
{
    if (value == maskLayer())
        return;
    GraphicsLayer::setMaskLayer(value);
    notifyChange(MaskLayerChange);

    if (!value)
        return;
    value->setSize(size());
    value->setContentsVisible(contentsAreVisible());
}


void GraphicsLayerTextureMapper::setReplicatedByLayer(GraphicsLayer* value)
{
    if (value == replicaLayer())
        return;
    GraphicsLayer::setReplicatedByLayer(value);
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

void GraphicsLayerTextureMapper::setContentsToSolidColor(const Color& color)
{
    if (color == m_solidColor)
        return;

    m_solidColor = color;
    notifyChange(BackgroundColorChange);
}

void GraphicsLayerTextureMapper::setContentsToImage(Image* image)
{
    if (image) {
        // Make the decision about whether the image has changed.
        // This code makes the assumption that pointer equality on a NativeImagePtr is a valid way to tell if the image is changed.
        // This assumption is true in Qt, GTK and EFL.
        NativeImagePtr newNativeImagePtr = image->nativeImageForCurrentFrame();
        if (!newNativeImagePtr)
            return;

        if (newNativeImagePtr == m_compositedNativeImagePtr)
            return;

        m_compositedNativeImagePtr = newNativeImagePtr;
        if (!m_compositedImage)
            m_compositedImage = TextureMapperTiledBackingStore::create();
        m_compositedImage->setContentsToImage(image);
        m_compositedImage->updateContentsScale(pageScaleFactor() * deviceScaleFactor());
    } else {
        m_compositedNativeImagePtr = nullptr;
        m_compositedImage = nullptr;
    }

    setContentsToPlatformLayer(m_compositedImage.get(), ContentsLayerForImage);
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
    notifyChange(DebugVisualsChange);
}

void GraphicsLayerTextureMapper::didCommitScrollOffset(const IntSize& offset)
{
    if (offset.isZero())
        return;

    m_committedScrollOffset = offset;
    notifyChange(CommittedScrollOffsetChange);
}

void GraphicsLayerTextureMapper::setIsScrollable(bool isScrollable)
{
    if (m_isScrollable == isScrollable)
        return;

    m_isScrollable = isScrollable;
    notifyChange(IsScrollableChange);
}

void GraphicsLayerTextureMapper::flushCompositingStateForThisLayerOnly(bool)
{
    prepareBackingStoreIfNeeded();
    commitLayerChanges();
    m_layer.syncAnimations();
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

    if (m_changeMask & ChildrenChange)
        m_layer.setChildren(children());

    if (m_changeMask & MaskLayerChange)
        m_layer.setMaskLayer(&downcast<GraphicsLayerTextureMapper>(maskLayer())->layer());

    if (m_changeMask & ReplicaLayerChange)
        m_layer.setReplicaLayer(&downcast<GraphicsLayerTextureMapper>(replicaLayer())->layer());

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

    if (m_changeMask & ContentsRectChange)
        m_layer.setContentsRect(contentsRect());

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

    if (m_changeMask & OpacityChange)
        m_layer.setOpacity(opacity());

    if (m_changeMask & BackgroundColorChange)
        m_layer.setSolidColor(m_solidColor);

    if (m_changeMask & FilterChange)
        m_layer.setFilters(filters());

    if (m_changeMask & BackingStoreChange)
        m_layer.setBackingStore(m_backingStore);

    if (m_changeMask & DebugVisualsChange)
        m_layer.setDebugVisuals(isShowingDebugBorder(), debugBorderColor(), debugBorderWidth(), isShowingRepaintCounter());

    if (m_changeMask & RepaintCountChange)
        m_layer.setRepaintCount(repaintCount());

    if (m_changeMask & ContentChange)
        m_layer.setContentsLayer(platformLayer());

    if (m_changeMask & AnimationChange)
        m_layer.setAnimations(m_animations);

    if (m_changeMask & AnimationStarted)
        client().notifyAnimationStarted(this, "", m_animationStartTime);

    if (m_changeMask & FixedToViewporChange)
        m_layer.setFixedToViewport(fixedToViewport());

    if (m_changeMask & IsScrollableChange)
        m_layer.setIsScrollable(isScrollable());

    if (m_changeMask & CommittedScrollOffsetChange)
        m_layer.didCommitScrollOffset(m_committedScrollOffset);

    m_changeMask = NoChanges;
}

void GraphicsLayerTextureMapper::flushCompositingState(const FloatRect& rect, bool viewportIsStable)
{
    if (!m_layer.textureMapper())
        return;

    flushCompositingStateForThisLayerOnly(viewportIsStable);

    if (maskLayer())
        maskLayer()->flushCompositingState(rect, viewportIsStable);
    if (replicaLayer())
        replicaLayer()->flushCompositingState(rect, viewportIsStable);
    for (auto* child : children())
        child->flushCompositingState(rect, viewportIsStable);
}

void GraphicsLayerTextureMapper::updateBackingStoreIncludingSubLayers()
{
    if (!m_layer.textureMapper())
        return;

    updateBackingStoreIfNeeded();

    if (maskLayer())
        downcast<GraphicsLayerTextureMapper>(*maskLayer()).updateBackingStoreIfNeeded();
    if (replicaLayer())
        downcast<GraphicsLayerTextureMapper>(*replicaLayer()).updateBackingStoreIfNeeded();
    for (auto* child : children())
        downcast<GraphicsLayerTextureMapper>(*child).updateBackingStoreIncludingSubLayers();
}

void GraphicsLayerTextureMapper::updateBackingStoreIfNeeded()
{
    TextureMapper* textureMapper = m_layer.textureMapper();
    if (!textureMapper)
        return;

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

    TextureMapperTiledBackingStore* backingStore = static_cast<TextureMapperTiledBackingStore*>(m_backingStore.get());
    backingStore->updateContentsScale(pageScaleFactor() * deviceScaleFactor());

    dirtyRect.scale(pageScaleFactor() * deviceScaleFactor());
    backingStore->updateContents(*textureMapper, this, m_size, dirtyRect, BitmapTexture::UpdateCanModifyOriginalImageData);

    m_needsDisplay = false;
    m_needsDisplayRect = IntRect();
}

bool GraphicsLayerTextureMapper::shouldHaveBackingStore() const
{
    return drawsContent() && contentsAreVisible() && !m_size.isEmpty();
}

bool GraphicsLayerTextureMapper::addAnimation(const KeyframeValueList& valueList, const FloatSize& boxSize, const Animation* anim, const String& keyframesName, double timeOffset)
{
    ASSERT(!keyframesName.isEmpty());

    if (!anim || anim->isEmptyOrZeroDuration() || valueList.size() < 2 || (valueList.property() != AnimatedPropertyTransform && valueList.property() != AnimatedPropertyOpacity))
        return false;

    bool listsMatch = false;
    bool hasBigRotation;

    if (valueList.property() == AnimatedPropertyTransform)
        listsMatch = validateTransformOperations(valueList, hasBigRotation) >= 0;

    const double currentTime = monotonicallyIncreasingTime();
    m_animations.add(TextureMapperAnimation(keyframesName, valueList, boxSize, anim, currentTime - timeOffset, listsMatch));
    // m_animationStartTime is the time of the first real frame of animation, now or delayed by a negative offset.
    if (timeOffset > 0)
        m_animationStartTime = currentTime;
    else
        m_animationStartTime = currentTime - timeOffset;
    notifyChange(AnimationChange);
    notifyChange(AnimationStarted);
    return true;
}

void GraphicsLayerTextureMapper::setAnimations(const TextureMapperAnimations& animations)
{
    m_animations = animations;
    notifyChange(AnimationChange);
}


void GraphicsLayerTextureMapper::pauseAnimation(const String& animationName, double timeOffset)
{
    m_animations.pause(animationName, timeOffset);
}

void GraphicsLayerTextureMapper::removeAnimation(const String& animationName)
{
    m_animations.remove(animationName);
}

bool GraphicsLayerTextureMapper::setFilters(const FilterOperations& filters)
{
    TextureMapper* textureMapper = m_layer.textureMapper();
    if (!textureMapper)
        return false;
    notifyChange(FilterChange);
    return GraphicsLayer::setFilters(filters);
}

void GraphicsLayerTextureMapper::setFixedToViewport(bool fixed)
{
    if (m_fixedToViewport == fixed)
        return;

    m_fixedToViewport = fixed;
    notifyChange(FixedToViewporChange);
}

void GraphicsLayerTextureMapper::setRepaintCount(int repaintCount)
{
    m_repaintCount = repaintCount;
    notifyChange(RepaintCountChange);
}

}
#endif
