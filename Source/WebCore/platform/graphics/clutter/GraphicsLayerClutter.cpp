/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Collabora Ltd.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)
#include "GraphicsLayerClutter.h"

#include "FloatRect.h"
#include "GraphicsLayerActor.h"
#include "GraphicsLayerFactory.h"
#include "NotImplemented.h"
#include "RefPtrCairo.h"
#include "TransformState.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// This is the hook for WebCore compositor to know that the webKit clutter port implements
// compositing with GraphicsLayerClutter.
PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerFactory* factory, GraphicsLayerClient* client)
{
    if (!factory)
        return adoptPtr(new GraphicsLayerClutter(client));

    return factory->createGraphicsLayer(client);
}

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerClient* client)
{
    return adoptPtr(new GraphicsLayerClutter(client));
}

GraphicsLayerClutter::GraphicsLayerClutter(GraphicsLayerClient* client)
    : GraphicsLayer(client)
    , m_uncommittedChanges(0)
{
    // ClutterRectangle will be used to show the debug border.
    m_layer = graphicsLayerActorNewWithClient(LayerTypeWebLayer, this);
}

static gboolean idleDestroy(gpointer data)
{
    GRefPtr<ClutterActor> actor = adoptGRef(CLUTTER_ACTOR(data));
    ClutterActor* parent = clutter_actor_get_parent(actor.get());

    // We should remove child actors manually because the container of Clutter 
    // seems to have a bug to remove its child actors when it is removed. 
    if (GRAPHICS_LAYER_IS_ACTOR(GRAPHICS_LAYER_ACTOR(actor.get())))
        graphicsLayerActorRemoveAll(GRAPHICS_LAYER_ACTOR(actor.get()));

    if (parent)
        clutter_container_remove_actor(CLUTTER_CONTAINER(parent), actor.get());

    // FIXME: we should assert that the actor's ref count is 1 here, but some
    // of them are getting here with 2!
    // ASSERT((G_OBJECT(actor.get()))->ref_count == 1);

    return FALSE;
}

GraphicsLayerClutter::~GraphicsLayerClutter()
{
    if (graphicsLayerActorGetLayerType(m_layer.get()) == GraphicsLayerClutter::LayerTypeRootLayer)
        return;

    // We destroy the actors on an idle so that the main loop can run enough to
    // repaint the background that will replace the actor.
    if (m_layer) {
        graphicsLayerActorSetClient(m_layer.get(), 0);
        g_idle_add(idleDestroy, m_layer.leakRef());
    }
}

void GraphicsLayerClutter::setName(const String& name)
{
    String longName = String::format("Actor(%p) GraphicsLayer(%p) ", m_layer.get(), this) + name;
    GraphicsLayer::setName(longName);
    noteLayerPropertyChanged(NameChanged);
}

ClutterActor* GraphicsLayerClutter::platformLayer() const
{
    return CLUTTER_ACTOR(m_layer.get());
}

void GraphicsLayerClutter::setNeedsDisplay()
{
    FloatRect hugeRect(FloatPoint(), m_size);
    setNeedsDisplayInRect(hugeRect);
}

void GraphicsLayerClutter::setNeedsDisplayInRect(const FloatRect& r)
{
    if (!drawsContent())
        return;

    FloatRect rect(r);
    FloatRect layerBounds(FloatPoint(), m_size);
    rect.intersect(layerBounds);
    if (rect.isEmpty())
        return;

    const size_t maxDirtyRects = 32;

    for (size_t i = 0; i < m_dirtyRects.size(); ++i) {
        if (m_dirtyRects[i].contains(rect))
            return;
    }

    if (m_dirtyRects.size() < maxDirtyRects)
        m_dirtyRects.append(rect);
    else
        m_dirtyRects[0].unite(rect);

    noteLayerPropertyChanged(DirtyRectsChanged);
}

void GraphicsLayerClutter::setAnchorPoint(const FloatPoint3D& point)
{
    if (point == m_anchorPoint)
        return;

    GraphicsLayer::setAnchorPoint(point);
    noteLayerPropertyChanged(GeometryChanged);
}

void GraphicsLayerClutter::setPosition(const FloatPoint& point)
{
    if (point == m_position)
        return;

    GraphicsLayer::setPosition(point);
    noteLayerPropertyChanged(GeometryChanged);
}

void GraphicsLayerClutter::setSize(const FloatSize& size)
{
    if (size == m_size)
        return;

    GraphicsLayer::setSize(size);
    noteLayerPropertyChanged(GeometryChanged);
}
void GraphicsLayerClutter::setTransform(const TransformationMatrix& t)
{
    if (t == m_transform)
        return;

    GraphicsLayer::setTransform(t);
    noteLayerPropertyChanged(TransformChanged);
}

void GraphicsLayerClutter::setDrawsContent(bool drawsContent)
{
    if (drawsContent == m_drawsContent)
        return;

    GraphicsLayer::setDrawsContent(drawsContent);
    noteLayerPropertyChanged(DrawsContentChanged);
}

void GraphicsLayerClutter::setParent(GraphicsLayer* childLayer)
{
    notImplemented();

    GraphicsLayer::setParent(childLayer);
}

bool GraphicsLayerClutter::setChildren(const Vector<GraphicsLayer*>& children)
{
    bool childrenChanged = GraphicsLayer::setChildren(children);
    if (childrenChanged)
        noteSublayersChanged();

    return childrenChanged;
}

void GraphicsLayerClutter::addChild(GraphicsLayer* childLayer)
{
    GraphicsLayer::addChild(childLayer);
    noteSublayersChanged();
}

void GraphicsLayerClutter::addChildAtIndex(GraphicsLayer* childLayer, int index)
{
    GraphicsLayer::addChildAtIndex(childLayer, index);
    noteSublayersChanged();
}

void GraphicsLayerClutter::addChildBelow(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(childLayer, sibling);
    noteSublayersChanged();
}

void GraphicsLayerClutter::addChildAbove(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildAbove(childLayer, sibling);
    noteSublayersChanged();
}

bool GraphicsLayerClutter::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    if (GraphicsLayer::replaceChild(oldChild, newChild)) {
        noteSublayersChanged();
        return true;
    }
    return false;
}

void GraphicsLayerClutter::removeFromParent()
{
    if (m_parent)
        static_cast<GraphicsLayerClutter*>(m_parent)->noteSublayersChanged();
    GraphicsLayer::removeFromParent();
}

void GraphicsLayerClutter::platformClutterLayerPaintContents(GraphicsContext& context, const IntRect& clip)
{
    paintGraphicsLayerContents(context, clip);
}

void GraphicsLayerClutter::platformClutterLayerAnimationStarted(double startTime)
{
    if (m_client)
        m_client->notifyAnimationStarted(this, startTime);
}

void GraphicsLayerClutter::repaintLayerDirtyRects()
{
    if (!m_dirtyRects.size())
        return;

    for (size_t i = 0; i < m_dirtyRects.size(); ++i)
        graphicsLayerActorInvalidateRectangle(m_layer.get(), m_dirtyRects[i]);

    m_dirtyRects.clear();
}

FloatPoint GraphicsLayerClutter::computePositionRelativeToBase(float& pageScale) const
{
    pageScale = 1;

    FloatPoint offset;
    for (const GraphicsLayer* currLayer = this; currLayer; currLayer = currLayer->parent()) {
        if (currLayer->appliesPageScale()) {
            if (currLayer->client())
                pageScale = currLayer->pageScaleFactor();
            return offset;
        }

        offset += currLayer->position();
    }

    return FloatPoint();
}

// called from void RenderLayerCompositor::flushPendingLayerChanges
void GraphicsLayerClutter::flushCompositingState(const FloatRect& clipRect)
{
    TransformState state(TransformState::UnapplyInverseTransformDirection, FloatQuad(clipRect));
    recursiveCommitChanges(state);
}

void GraphicsLayerClutter::recursiveCommitChanges(const TransformState& state, float pageScaleFactor, const FloatPoint& positionRelativeToBase, bool affectedByPageScale)
{
    // FIXME: Save the state before sending down to kids and restore it after
    TransformState localState = state;

    if (appliesPageScale()) {
        pageScaleFactor = this->pageScaleFactor();
        affectedByPageScale = true;
    }

    // Accumulate an offset from the ancestral pixel-aligned layer.
    FloatPoint baseRelativePosition = positionRelativeToBase;
    if (affectedByPageScale)
        baseRelativePosition += m_position;

    commitLayerChangesBeforeSublayers(pageScaleFactor, baseRelativePosition);

    const Vector<GraphicsLayer*>& childLayers = children();
    size_t numChildren = childLayers.size();

    for (size_t i = 0; i < numChildren; ++i) {
        GraphicsLayerClutter* curChild = static_cast<GraphicsLayerClutter*>(childLayers[i]);
        curChild->recursiveCommitChanges(localState, pageScaleFactor, baseRelativePosition, affectedByPageScale);
    }

    commitLayerChangesAfterSublayers();
}

void GraphicsLayerClutter::flushCompositingStateForThisLayerOnly()
{
    float pageScaleFactor;
    FloatPoint offset = computePositionRelativeToBase(pageScaleFactor);
    commitLayerChangesBeforeSublayers(pageScaleFactor, offset);
    commitLayerChangesAfterSublayers();
}

void GraphicsLayerClutter::commitLayerChangesAfterSublayers()
{
    if (!m_uncommittedChanges)
        return;

    m_uncommittedChanges = NoChange;
}
void GraphicsLayerClutter::noteSublayersChanged()
{
    noteLayerPropertyChanged(ChildrenChanged);
}

void GraphicsLayerClutter::noteLayerPropertyChanged(LayerChangeFlags flags)
{
    if (!m_uncommittedChanges && m_client)
        m_client->notifyFlushRequired(this); // call RenderLayerBacking::notifyFlushRequired

    m_uncommittedChanges |= flags;
}

void GraphicsLayerClutter::commitLayerChangesBeforeSublayers(float pageScaleFactor, const FloatPoint& positionRelativeToBase)
{
    if (!m_uncommittedChanges)
        return;

    if (m_uncommittedChanges & NameChanged)
        updateLayerNames();

    if (m_uncommittedChanges & ChildrenChanged)
        updateSublayerList();

    if (m_uncommittedChanges & GeometryChanged)
        updateGeometry(pageScaleFactor, positionRelativeToBase);

    if (m_uncommittedChanges & TransformChanged)
        updateTransform();

    if (m_uncommittedChanges & DrawsContentChanged)
        updateLayerDrawsContent(pageScaleFactor, positionRelativeToBase);

    if (m_uncommittedChanges & DirtyRectsChanged)
        repaintLayerDirtyRects();
}

void GraphicsLayerClutter::updateGeometry(float pageScaleFactor, const FloatPoint& positionRelativeToBase)
{
    FloatPoint scaledPosition;
    FloatPoint3D scaledAnchorPoint;
    FloatSize scaledSize;

    // FIXME: Need to support scaling
    scaledPosition = m_position;
    scaledAnchorPoint = m_anchorPoint;
    scaledSize = m_size;

    FloatRect adjustedBounds(m_boundsOrigin , scaledSize);
    FloatPoint adjustedPosition(scaledPosition.x() + scaledAnchorPoint.x() * scaledSize.width(), scaledPosition.y() + scaledAnchorPoint.y() * scaledSize.height());

    clutter_actor_set_size(CLUTTER_ACTOR(m_layer.get()), adjustedBounds.width(), adjustedBounds.height());
    clutter_actor_set_position(CLUTTER_ACTOR(m_layer.get()), adjustedPosition.x(), adjustedPosition.y());
    graphicsLayerActorSetAnchorPoint(m_layer.get(), scaledAnchorPoint.x(), scaledAnchorPoint.y(), scaledAnchorPoint.z());
}

// Each GraphicsLayer has the corresponding layer in the platform port.
// So whenever the list of child layer changes, the list of GraphicsLayerActor should be updated accordingly.
void GraphicsLayerClutter::updateSublayerList()
{
    GraphicsLayerActorList newSublayers;
    const Vector<GraphicsLayer*>& childLayers = children();

    if (childLayers.size() > 0) {
        size_t numChildren = childLayers.size();
        for (size_t i = 0; i < numChildren; ++i) {
            GraphicsLayerClutter* curChild = static_cast<GraphicsLayerClutter*>(childLayers[i]);
            GraphicsLayerActor* childLayer = curChild->layerForSuperlayer();
            g_assert(GRAPHICS_LAYER_IS_ACTOR(childLayer));
            newSublayers.append(childLayer);
        }

        for (size_t i = 0; i < newSublayers.size(); i++) {
            ClutterActor* layerActor = CLUTTER_ACTOR(newSublayers[i].get());
            ClutterActor* parentActor = clutter_actor_get_parent(layerActor);
            if (parentActor)
                clutter_container_remove_actor(CLUTTER_CONTAINER(parentActor), layerActor);
        }
    }

    graphicsLayerActorSetSublayers(m_layer.get(), newSublayers);
}

void GraphicsLayerClutter::updateLayerNames()
{
    clutter_actor_set_name(CLUTTER_ACTOR(m_layer.get()), name().utf8().data());
}

void GraphicsLayerClutter::updateTransform()
{
    CoglMatrix matrix = m_transform;
    graphicsLayerActorSetTransform(primaryLayer(), &matrix);
}

void GraphicsLayerClutter::updateLayerDrawsContent(float pageScaleFactor, const FloatPoint& positionRelativeToBase)
{
    if (m_drawsContent) {
        graphicsLayerActorSetDrawsContent(m_layer.get(), TRUE);
        setNeedsDisplay();
    } else {
        graphicsLayerActorSetDrawsContent(m_layer.get(), FALSE);
        graphicsLayerActorSetSurface(m_layer.get(), 0);
    }

    updateDebugIndicators();
}

GraphicsLayerActor* GraphicsLayerClutter::layerForSuperlayer() const
{
    return m_layer.get();
}
} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
