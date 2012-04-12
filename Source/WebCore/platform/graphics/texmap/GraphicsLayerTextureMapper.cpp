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

#include "TextureMapperLayer.h"

namespace WebCore {

GraphicsLayerTextureMapper::GraphicsLayerTextureMapper(GraphicsLayerClient* client)
    : GraphicsLayer(client)
    , m_layer(adoptPtr(new TextureMapperLayer()))
    , m_changeMask(0)
    , m_needsDisplay(false)
    , m_fixedToViewport(false)
    , m_contentsLayer(0)
    , m_animationStartedTimer(this, &GraphicsLayerTextureMapper::animationStartedTimerFired)
{
}

void GraphicsLayerTextureMapper::notifyChange(TextureMapperLayer::ChangeMask changeMask)
{
    m_changeMask |= changeMask;
    if (!client())
        return;
    client()->notifySyncRequired(this);
}

void GraphicsLayerTextureMapper::didSynchronize()
{
    m_syncQueued = false;
    m_changeMask = 0;
    m_needsDisplay = false;
    m_needsDisplayRect = IntRect();
}

void GraphicsLayerTextureMapper::setName(const String& name)
{
    GraphicsLayer::setName(name);
}

GraphicsLayerTextureMapper::~GraphicsLayerTextureMapper()
{
    willBeDestroyed();
}

void GraphicsLayerTextureMapper::willBeDestroyed()
{
    GraphicsLayer::willBeDestroyed();
}

/* \reimp (GraphicsLayer.h): The current size might change, thus we need to update the whole display.
*/
void GraphicsLayerTextureMapper::setNeedsDisplay()
{
    m_needsDisplay = true;
    notifyChange(TextureMapperLayer::DisplayChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setContentsNeedsDisplay()
{
    if (m_image)
        setContentsToImage(m_image.get());
    notifyChange(TextureMapperLayer::DisplayChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setNeedsDisplayInRect(const FloatRect& rect)
{
    if (m_needsDisplay)
        return;
    m_needsDisplayRect.unite(rect);
    notifyChange(TextureMapperLayer::DisplayChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setParent(GraphicsLayer* layer)
{
    notifyChange(TextureMapperLayer::ParentChange);
    GraphicsLayer::setParent(layer);
}

/* \reimp (GraphicsLayer.h)
*/
bool GraphicsLayerTextureMapper::setChildren(const Vector<GraphicsLayer*>& children)
{
    notifyChange(TextureMapperLayer::ChildrenChange);
    return GraphicsLayer::setChildren(children);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::addChild(GraphicsLayer* layer)
{
    notifyChange(TextureMapperLayer::ChildrenChange);
    GraphicsLayer::addChild(layer);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::addChildAtIndex(GraphicsLayer* layer, int index)
{
    GraphicsLayer::addChildAtIndex(layer, index);
    notifyChange(TextureMapperLayer::ChildrenChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::addChildAbove(GraphicsLayer* layer, GraphicsLayer* sibling)
{
     GraphicsLayer::addChildAbove(layer, sibling);
     notifyChange(TextureMapperLayer::ChildrenChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::addChildBelow(GraphicsLayer* layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(layer, sibling);
    notifyChange(TextureMapperLayer::ChildrenChange);
}

/* \reimp (GraphicsLayer.h)
*/
bool GraphicsLayerTextureMapper::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    if (GraphicsLayer::replaceChild(oldChild, newChild)) {
        notifyChange(TextureMapperLayer::ChildrenChange);
        return true;
    }
    return false;
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::removeFromParent()
{
    if (!parent())
        return;
    notifyChange(TextureMapperLayer::ParentChange);
    GraphicsLayer::removeFromParent();
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setMaskLayer(GraphicsLayer* value)
{
    if (value == maskLayer())
        return;
    GraphicsLayer::setMaskLayer(value);
    notifyChange(TextureMapperLayer::MaskLayerChange);
}


/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setReplicatedByLayer(GraphicsLayer* value)
{
    if (value == replicaLayer())
        return;
    GraphicsLayer::setReplicatedByLayer(value);
    notifyChange(TextureMapperLayer::ReplicaLayerChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setPosition(const FloatPoint& value)
{
    if (value == position())
        return;
    GraphicsLayer::setPosition(value);
    notifyChange(TextureMapperLayer::PositionChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setAnchorPoint(const FloatPoint3D& value)
{
    if (value == anchorPoint())
        return;
    GraphicsLayer::setAnchorPoint(value);
    notifyChange(TextureMapperLayer::AnchorPointChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setSize(const FloatSize& value)
{
    if (value == size())
        return;

    GraphicsLayer::setSize(value);
    notifyChange(TextureMapperLayer::SizeChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setTransform(const TransformationMatrix& value)
{
    if (value == transform())
        return;

    GraphicsLayer::setTransform(value);
    notifyChange(TextureMapperLayer::TransformChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setChildrenTransform(const TransformationMatrix& value)
{
    if (value == childrenTransform())
        return;
    GraphicsLayer::setChildrenTransform(value);
    notifyChange(TextureMapperLayer::ChildrenTransformChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setPreserves3D(bool value)
{
    if (value == preserves3D())
        return;
    GraphicsLayer::setPreserves3D(value);
    notifyChange(TextureMapperLayer::Preserves3DChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setMasksToBounds(bool value)
{
    if (value == masksToBounds())
        return;
    GraphicsLayer::setMasksToBounds(value);
    notifyChange(TextureMapperLayer::MasksToBoundsChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setDrawsContent(bool value)
{
    if (value == drawsContent())
        return;
    notifyChange(TextureMapperLayer::DrawsContentChange);
    GraphicsLayer::setDrawsContent(value);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setContentsOpaque(bool value)
{
    if (value == contentsOpaque())
        return;
    notifyChange(TextureMapperLayer::ContentsOpaqueChange);
    GraphicsLayer::setContentsOpaque(value);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setBackfaceVisibility(bool value)
{
    if (value == backfaceVisibility())
        return;
    GraphicsLayer::setBackfaceVisibility(value);
    notifyChange(TextureMapperLayer::BackfaceVisibilityChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setOpacity(float value)
{
    if (value == opacity())
        return;
    GraphicsLayer::setOpacity(value);
    notifyChange(TextureMapperLayer::OpacityChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setContentsRect(const IntRect& value)
{
    if (value == contentsRect())
        return;
    GraphicsLayer::setContentsRect(value);
    notifyChange(TextureMapperLayer::ContentsRectChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setContentsToImage(Image* image)
{
    if (image == m_image)
        return;

    m_image = image;
    if (m_image) {
        RefPtr<TextureMapperTiledBackingStore> backingStore = TextureMapperTiledBackingStore::create();
        backingStore->setContentsToImage(image);
        m_compositedImage = backingStore;
    } else
        m_compositedImage = 0;

    setContentsToMedia(m_compositedImage.get());
    notifyChange(TextureMapperLayer::ContentChange);
    GraphicsLayer::setContentsToImage(image);
}

void GraphicsLayerTextureMapper::setContentsToMedia(TextureMapperPlatformLayer* media)
{
    if (media == m_contentsLayer)
        return;

    GraphicsLayer::setContentsToMedia(media);
    notifyChange(TextureMapperLayer::ContentChange);
    m_contentsLayer = media;
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::syncCompositingStateForThisLayerOnly()
{
    m_layer->syncCompositingState(this);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::syncCompositingState(const FloatRect&)
{
    m_layer->syncCompositingState(this, TextureMapperLayer::TraverseDescendants);
}

bool GraphicsLayerTextureMapper::addAnimation(const KeyframeValueList& valueList, const IntSize& boxSize, const Animation* anim, const String& keyframesName, double timeOffset)
{
    ASSERT(!keyframesName.isEmpty());

    if (!anim || anim->isEmptyOrZeroDuration() || valueList.size() < 2 || (valueList.property() != AnimatedPropertyWebkitTransform && valueList.property() != AnimatedPropertyOpacity))
        return false;

    bool listsMatch = false;
    bool hasBigRotation;

    if (valueList.property() == AnimatedPropertyWebkitTransform)
        listsMatch = validateTransformOperations(valueList, hasBigRotation) >= 0;

    m_animations.add(keyframesName, TextureMapperAnimation(valueList, boxSize, anim, timeOffset, listsMatch));
    notifyChange(TextureMapperLayer::AnimationChange);
    m_animationStartedTimer.startOneShot(0);
    return true;
}

void GraphicsLayerTextureMapper::pauseAnimation(const String& animationName, double timeOffset)
{
    m_animations.pause(animationName, timeOffset);
}

void GraphicsLayerTextureMapper::removeAnimation(const String& animationName)
{
    m_animations.remove(animationName);
}

void GraphicsLayerTextureMapper::animationStartedTimerFired(Timer<GraphicsLayerTextureMapper>*)
{
    client()->notifyAnimationStarted(this, /* DOM time */ WTF::currentTime());
}

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerClient* client)
{
    if (s_graphicsLayerFactory)
        return (*s_graphicsLayerFactory)(client);
    return adoptPtr(new GraphicsLayerTextureMapper(client));
}

#if ENABLE(CSS_FILTERS)
bool GraphicsLayerTextureMapper::setFilters(const FilterOperations& filters)
{
    notifyChange(TextureMapperLayer::FilterChange);
    return GraphicsLayer::setFilters(filters);
}
#endif

}
