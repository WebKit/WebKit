/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011, 2012, 2013 Collabora Ltd.
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

#include "Animation.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "GraphicsLayerActor.h"
#include "GraphicsLayerFactory.h"
#include "NotImplemented.h"
#include "RefPtrCairo.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "TransformState.h"
#include "TranslateTransformOperation.h"
#include <limits.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

using namespace std;

namespace WebCore {

// If we send a duration of 0 to ClutterTimeline, then it will fail to set the duration. 
// So send a very small value instead.
static const float cAnimationAlmostZeroDuration = 1e-3f;

static bool isTransformTypeTransformationMatrix(TransformOperation::OperationType transformType)
{
    switch (transformType) {
    case TransformOperation::SKEW_X:
    case TransformOperation::SKEW_Y:
    case TransformOperation::SKEW:
    case TransformOperation::MATRIX:
    case TransformOperation::ROTATE_3D:
    case TransformOperation::MATRIX_3D:
    case TransformOperation::PERSPECTIVE:
    case TransformOperation::IDENTITY:
    case TransformOperation::NONE:
        return true;
    default:
        return false;
    }
}

static bool isTransformTypeFloatPoint3D(TransformOperation::OperationType transformType)
{
    switch (transformType) {
    case TransformOperation::SCALE:
    case TransformOperation::SCALE_3D:
    case TransformOperation::TRANSLATE:
    case TransformOperation::TRANSLATE_3D:
        return true;
    default:
        return false;
    }
}

static bool isTransformTypeNumber(TransformOperation::OperationType transformType)
{
    return !isTransformTypeTransformationMatrix(transformType) && !isTransformTypeFloatPoint3D(transformType);
}

static void getTransformFunctionValue(const TransformOperation* transformOp, TransformOperation::OperationType transformType, const IntSize& size, float& value)
{
    switch (transformType) {
    case TransformOperation::ROTATE:
    case TransformOperation::ROTATE_X:
    case TransformOperation::ROTATE_Y:
        value = transformOp ? narrowPrecisionToFloat(deg2rad(static_cast<const RotateTransformOperation*>(transformOp)->angle())) : 0;
        break;
    case TransformOperation::SCALE_X:
        value = transformOp ? narrowPrecisionToFloat(static_cast<const ScaleTransformOperation*>(transformOp)->x()) : 1;
        break;
    case TransformOperation::SCALE_Y:
        value = transformOp ? narrowPrecisionToFloat(static_cast<const ScaleTransformOperation*>(transformOp)->y()) : 1;
        break;
    case TransformOperation::SCALE_Z:
        value = transformOp ? narrowPrecisionToFloat(static_cast<const ScaleTransformOperation*>(transformOp)->z()) : 1;
        break;
    case TransformOperation::TRANSLATE_X:
        value = transformOp ? narrowPrecisionToFloat(static_cast<const TranslateTransformOperation*>(transformOp)->x(size)) : 0;
        break;
    case TransformOperation::TRANSLATE_Y:
        value = transformOp ? narrowPrecisionToFloat(static_cast<const TranslateTransformOperation*>(transformOp)->y(size)) : 0;
        break;
    case TransformOperation::TRANSLATE_Z:
        value = transformOp ? narrowPrecisionToFloat(static_cast<const TranslateTransformOperation*>(transformOp)->z(size)) : 0;
        break;
    default:
        break;
    }
}

static void getTransformFunctionValue(const TransformOperation* transformOp, TransformOperation::OperationType transformType, const IntSize& size, FloatPoint3D& value)
{
    switch (transformType) {
    case TransformOperation::SCALE:
    case TransformOperation::SCALE_3D:
        value.setX(transformOp ? narrowPrecisionToFloat(static_cast<const ScaleTransformOperation*>(transformOp)->x()) : 1);
        value.setY(transformOp ? narrowPrecisionToFloat(static_cast<const ScaleTransformOperation*>(transformOp)->y()) : 1);
        value.setZ(transformOp ? narrowPrecisionToFloat(static_cast<const ScaleTransformOperation*>(transformOp)->z()) : 1);
        break;
    case TransformOperation::TRANSLATE:
    case TransformOperation::TRANSLATE_3D:
        value.setX(transformOp ? narrowPrecisionToFloat(static_cast<const TranslateTransformOperation*>(transformOp)->x(size)) : 0);
        value.setY(transformOp ? narrowPrecisionToFloat(static_cast<const TranslateTransformOperation*>(transformOp)->y(size)) : 0);
        value.setZ(transformOp ? narrowPrecisionToFloat(static_cast<const TranslateTransformOperation*>(transformOp)->z(size)) : 0);
        break;
    default:
        break;
    }
}

static void getTransformFunctionValue(const TransformOperation* transformOp, TransformOperation::OperationType transformType, const IntSize& size, TransformationMatrix& value)
{
    switch (transformType) {
    case TransformOperation::SKEW_X:
    case TransformOperation::SKEW_Y:
    case TransformOperation::SKEW:
    case TransformOperation::MATRIX:
    case TransformOperation::ROTATE_3D:
    case TransformOperation::MATRIX_3D:
    case TransformOperation::PERSPECTIVE:
    case TransformOperation::IDENTITY:
    case TransformOperation::NONE:
        if (transformOp)
            transformOp->apply(value, size);
        else
            value.makeIdentity();
        break;
    default:
        break;
    }
}

static PlatformClutterAnimation::ValueFunctionType getValueFunctionNameForTransformOperation(TransformOperation::OperationType transformType)
{
    // Use literal strings to avoid link-time dependency on those symbols.
    switch (transformType) {
    case TransformOperation::ROTATE_X:
        return PlatformClutterAnimation::RotateX;
    case TransformOperation::ROTATE_Y:
        return PlatformClutterAnimation::RotateY;
    case TransformOperation::ROTATE:
        return PlatformClutterAnimation::RotateZ;
    case TransformOperation::SCALE_X:
        return PlatformClutterAnimation::ScaleX;
    case TransformOperation::SCALE_Y:
        return PlatformClutterAnimation::ScaleY;
    case TransformOperation::SCALE_Z:
        return PlatformClutterAnimation::ScaleZ;
    case TransformOperation::TRANSLATE_X:
        return PlatformClutterAnimation::TranslateX;
    case TransformOperation::TRANSLATE_Y:
        return PlatformClutterAnimation::TranslateY;
    case TransformOperation::TRANSLATE_Z:
        return PlatformClutterAnimation::TranslateZ;
    case TransformOperation::SCALE:
    case TransformOperation::SCALE_3D:
        return PlatformClutterAnimation::Scale;
    case TransformOperation::TRANSLATE:
    case TransformOperation::TRANSLATE_3D:
        return PlatformClutterAnimation::Translate;
    case TransformOperation::MATRIX_3D:
        return PlatformClutterAnimation::Matrix;
    default:
        return PlatformClutterAnimation::NoValueFunction;
    }
}

static String propertyIdToString(AnimatedPropertyID property)
{
    switch (property) {
    case AnimatedPropertyWebkitTransform:
        return "transform";
    case AnimatedPropertyOpacity:
        return "opacity";
    case AnimatedPropertyBackgroundColor:
        return "backgroundColor";
    case AnimatedPropertyWebkitFilter:
        ASSERT_NOT_REACHED();
    case AnimatedPropertyInvalid:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return "";
}

static String animationIdentifier(const String& animationName, AnimatedPropertyID property, int index)
{
    return animationName + '_' + String::number(property) + '_' + String::number(index);
}

static bool animationHasStepsTimingFunction(const KeyframeValueList& valueList, const Animation* anim)
{
    if (anim->timingFunction()->isStepsTimingFunction())
        return true;

    for (unsigned i = 0; i < valueList.size(); ++i) {
        const TimingFunction* timingFunction = valueList.at(i)->timingFunction();
        if (timingFunction && timingFunction->isStepsTimingFunction())
            return true;
    }

    return false;
}

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
        clutter_actor_remove_child(parent, actor.get());

    // FIXME: we should assert that the actor's ref count is 1 here, but some
    // of them are getting here with 2!
    // ASSERT((G_OBJECT(actor.get()))->ref_count == 1);

    return FALSE;
}

GraphicsLayerClutter::~GraphicsLayerClutter()
{
    if (graphicsLayerActorGetLayerType(m_layer.get()) == GraphicsLayerClutter::LayerTypeRootLayer)
        return;

    willBeDestroyed();

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

void GraphicsLayerClutter::setOpacity(float opacity)
{
    float clampedOpacity = max(0.0f, min(opacity, 1.0f));
    if (clampedOpacity == m_opacity)
        return;

    GraphicsLayer::setOpacity(clampedOpacity);
    noteLayerPropertyChanged(OpacityChanged);
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

void GraphicsLayerClutter::updateOpacityOnLayer()
{
    clutter_actor_set_opacity(CLUTTER_ACTOR(primaryLayer()), static_cast<guint8>(roundf(m_opacity * 255)));
}

void GraphicsLayerClutter::updateAnimations()
{
    if (m_animationsToProcess.size()) {
        AnimationsToProcessMap::const_iterator end = m_animationsToProcess.end();
        for (AnimationsToProcessMap::const_iterator it = m_animationsToProcess.begin(); it != end; ++it) {
            const String& currAnimationName = it->key;
            AnimationsMap::iterator animationIt = m_runningAnimations.find(currAnimationName);
            if (animationIt == m_runningAnimations.end())
                continue;

            const AnimationProcessingAction& processingInfo = it->value;
            const Vector<LayerPropertyAnimation>& animations = animationIt->value;
            for (size_t i = 0; i < animations.size(); ++i) {
                const LayerPropertyAnimation& currAnimation = animations[i];
                switch (processingInfo.action) {
                case Remove:
                    removeClutterAnimationFromLayer(currAnimation.m_property, currAnimationName, currAnimation.m_index);
                    break;
                case Pause:
                    pauseClutterAnimationOnLayer(currAnimation.m_property, currAnimationName, currAnimation.m_index, processingInfo.timeOffset);
                    break;
                }
            }

            if (processingInfo.action == Remove)
                m_runningAnimations.remove(currAnimationName);
        }

        m_animationsToProcess.clear();
    }

    size_t numAnimations;
    if ((numAnimations = m_uncomittedAnimations.size())) {
        for (size_t i = 0; i < numAnimations; ++i) {
            const LayerPropertyAnimation& pendingAnimation = m_uncomittedAnimations[i];
            setAnimationOnLayer(pendingAnimation.m_animation.get(), pendingAnimation.m_property, pendingAnimation.m_name, pendingAnimation.m_index, pendingAnimation.m_timeOffset);

            AnimationsMap::iterator it = m_runningAnimations.find(pendingAnimation.m_name);
            if (it == m_runningAnimations.end()) {
                Vector<LayerPropertyAnimation> animations;
                animations.append(pendingAnimation);
                m_runningAnimations.add(pendingAnimation.m_name, animations);
            } else {
                Vector<LayerPropertyAnimation>& animations = it->value;
                animations.append(pendingAnimation);
            }
        }

        m_uncomittedAnimations.clear();
    }
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

    if (m_uncommittedChanges & OpacityChanged)
        updateOpacityOnLayer();

    if (m_uncommittedChanges & AnimationChanged)
        updateAnimations();
}

void GraphicsLayerClutter::updateGeometry(float pageScaleFactor, const FloatPoint& positionRelativeToBase)
{
    // FIXME: Need to support page scaling.
    clutter_actor_set_position(CLUTTER_ACTOR(m_layer.get()), m_position.x(), m_position.y());
    clutter_actor_set_size(CLUTTER_ACTOR(m_layer.get()), m_size.width(), m_size.height());
    graphicsLayerActorSetAnchorPoint(m_layer.get(), m_anchorPoint.x(), m_anchorPoint.y(), m_anchorPoint.z());
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
                clutter_actor_remove_child(parentActor, layerActor);
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

void GraphicsLayerClutter::setupAnimation(PlatformClutterAnimation* propertyAnim, const Animation* anim, bool additive)
{
    double duration = anim->duration();
    if (duration <= 0)
        duration = cAnimationAlmostZeroDuration;

    float repeatCount = anim->iterationCount();
    if (repeatCount == Animation::IterationCountInfinite)
        repeatCount = numeric_limits<float>::max();
    else if (anim->direction() == Animation::AnimationDirectionAlternate || anim->direction() == Animation::AnimationDirectionAlternateReverse)
        repeatCount /= 2;

    PlatformClutterAnimation::FillModeType fillMode = PlatformClutterAnimation::NoFillMode;
    switch (anim->fillMode()) {
    case AnimationFillModeNone:
        fillMode = PlatformClutterAnimation::Forwards; // Use "forwards" rather than "removed" because the style system will remove the animation when it is finished. This avoids a flash.
        break;
    case AnimationFillModeBackwards:
        fillMode = PlatformClutterAnimation::Both; // Use "both" rather than "backwards" because the style system will remove the animation when it is finished. This avoids a flash.
        break;
    case AnimationFillModeForwards:
        fillMode = PlatformClutterAnimation::Forwards;
        break;
    case AnimationFillModeBoth:
        fillMode = PlatformClutterAnimation::Both;
        break;
    }

    propertyAnim->setDuration(duration);
    propertyAnim->setRepeatCount(repeatCount);
    propertyAnim->setAutoreverses(anim->direction() == Animation::AnimationDirectionAlternate || anim->direction() == Animation::AnimationDirectionAlternateReverse);
    propertyAnim->setRemovedOnCompletion(false);
    propertyAnim->setAdditive(additive);
    propertyAnim->setFillMode(fillMode);
}

const TimingFunction* GraphicsLayerClutter::timingFunctionForAnimationValue(const AnimationValue* animValue, const Animation* anim)
{
    if (animValue->timingFunction())
        return animValue->timingFunction();
    if (anim->isTimingFunctionSet())
        return anim->timingFunction().get();

    return CubicBezierTimingFunction::defaultTimingFunction();
}

PassRefPtr<PlatformClutterAnimation> GraphicsLayerClutter::createBasicAnimation(const Animation* anim, const String& keyPath, bool additive)
{
    RefPtr<PlatformClutterAnimation> basicAnim = PlatformClutterAnimation::create(PlatformClutterAnimation::Basic, keyPath);
    setupAnimation(basicAnim.get(), anim, additive);
    return basicAnim;
}

PassRefPtr<PlatformClutterAnimation>GraphicsLayerClutter::createKeyframeAnimation(const Animation* anim, const String& keyPath, bool additive)
{
    RefPtr<PlatformClutterAnimation> keyframeAnim = PlatformClutterAnimation::create(PlatformClutterAnimation::Keyframe, keyPath);
    setupAnimation(keyframeAnim.get(), anim, additive);
    return keyframeAnim;
}

bool GraphicsLayerClutter::setTransformAnimationKeyframes(const KeyframeValueList& valueList, const Animation* animation, PlatformClutterAnimation* keyframeAnim, int functionIndex, TransformOperation::OperationType transformOpType, bool isMatrixAnimation, const IntSize& boxSize)
{
    Vector<float> keyTimes;
    Vector<float> floatValues;
    Vector<FloatPoint3D> floatPoint3DValues;
    Vector<TransformationMatrix> transformationMatrixValues;
    Vector<const TimingFunction*> timingFunctions;

    bool forwards = animation->directionIsForwards();

    for (unsigned i = 0; i < valueList.size(); ++i) {
        unsigned index = forwards ? i : (valueList.size() - i - 1);
        const TransformAnimationValue* curValue = static_cast<const TransformAnimationValue*>(valueList.at(index));
        keyTimes.append(forwards ? curValue->keyTime() : (1 - curValue->keyTime()));

        if (isMatrixAnimation) {
            TransformationMatrix transform;
            curValue->value()->apply(boxSize, transform);

            // FIXME: In CoreAnimation case, if any matrix is singular, CA won't animate it correctly.
            // But I'm not sure clutter also does. Check it later, and then decide
            // whether removing following lines or not.
            if (!transform.isInvertible())
                return false;

            transformationMatrixValues.append(transform);
        } else {
            const TransformOperation* transformOp = curValue->value()->at(functionIndex);
            if (isTransformTypeNumber(transformOpType)) {
                float value;
                getTransformFunctionValue(transformOp, transformOpType, boxSize, value);
                floatValues.append(value);
            } else if (isTransformTypeFloatPoint3D(transformOpType)) {
                FloatPoint3D value;
                getTransformFunctionValue(transformOp, transformOpType, boxSize, value);
                floatPoint3DValues.append(value);
            } else {
                TransformationMatrix value;
                getTransformFunctionValue(transformOp, transformOpType, boxSize, value);
                transformationMatrixValues.append(value);
            }
        }

        if (i < (valueList.size() - 1))
            timingFunctions.append(timingFunctionForAnimationValue(forwards ? curValue : valueList.at(index - 1), animation));
    }

    keyframeAnim->setKeyTimes(keyTimes);

    if (isTransformTypeNumber(transformOpType))
        keyframeAnim->setValues(floatValues);
    else if (isTransformTypeFloatPoint3D(transformOpType))
        keyframeAnim->setValues(floatPoint3DValues);
    else
        keyframeAnim->setValues(transformationMatrixValues);

    keyframeAnim->setTimingFunctions(timingFunctions, !forwards);

    PlatformClutterAnimation::ValueFunctionType valueFunction = getValueFunctionNameForTransformOperation(transformOpType);
    if (valueFunction != PlatformClutterAnimation::NoValueFunction)
        keyframeAnim->setValueFunction(valueFunction);

    return true;
}

bool GraphicsLayerClutter::setTransformAnimationEndpoints(const KeyframeValueList& valueList, const Animation* animation, PlatformClutterAnimation* basicAnim, int functionIndex, TransformOperation::OperationType transformOpType, bool isMatrixAnimation, const IntSize& boxSize)
{
    ASSERT(valueList.size() == 2);

    bool forwards = animation->directionIsForwards();

    unsigned fromIndex = !forwards;
    unsigned toIndex = forwards;

    const TransformAnimationValue* startValue = static_cast<const TransformAnimationValue*>(valueList.at(fromIndex));
    const TransformAnimationValue* endValue = static_cast<const TransformAnimationValue*>(valueList.at(toIndex));

    if (isMatrixAnimation) {
        TransformationMatrix fromTransform, toTransform;
        startValue->value()->apply(boxSize, fromTransform);
        endValue->value()->apply(boxSize, toTransform);

        // FIXME: If any matrix is singular, CA won't animate it correctly.
        // So fall back to software animation, But it's not sure in clutter case.
        // We need to investigate it more.
        if (!fromTransform.isInvertible() || !toTransform.isInvertible())
            return false;

        basicAnim->setFromValue(fromTransform);
        basicAnim->setToValue(toTransform);
    } else {
        if (isTransformTypeNumber(transformOpType)) {
            float fromValue;
            getTransformFunctionValue(startValue->value()->at(functionIndex), transformOpType, boxSize, fromValue);
            basicAnim->setFromValue(fromValue);

            float toValue;
            getTransformFunctionValue(endValue->value()->at(functionIndex), transformOpType, boxSize, toValue);
            basicAnim->setToValue(toValue);
        } else if (isTransformTypeFloatPoint3D(transformOpType)) {
            FloatPoint3D fromValue;
            getTransformFunctionValue(startValue->value()->at(functionIndex), transformOpType, boxSize, fromValue);
            basicAnim->setFromValue(fromValue);

            FloatPoint3D toValue;
            getTransformFunctionValue(endValue->value()->at(functionIndex), transformOpType, boxSize, toValue);
            basicAnim->setToValue(toValue);
        } else {
            TransformationMatrix fromValue;
            getTransformFunctionValue(startValue->value()->at(functionIndex), transformOpType, boxSize, fromValue);
            basicAnim->setFromValue(fromValue);

            TransformationMatrix toValue;
            getTransformFunctionValue(endValue->value()->at(functionIndex), transformOpType, boxSize, toValue);
            basicAnim->setToValue(toValue);
        }
    }

    // This codepath is used for 2-keyframe animations, so we still need to look in the start
    // for a timing function. Even in the reversing animation case, the first keyframe provides the timing function.
    const TimingFunction* timingFunction = timingFunctionForAnimationValue(valueList.at(0), animation);
    basicAnim->setTimingFunction(timingFunction, !forwards);

    PlatformClutterAnimation::ValueFunctionType valueFunction = getValueFunctionNameForTransformOperation(transformOpType);
    if (valueFunction != PlatformClutterAnimation::NoValueFunction)
        basicAnim->setValueFunction(valueFunction);

    return true;
}

bool GraphicsLayerClutter::appendToUncommittedAnimations(const KeyframeValueList& valueList, const TransformOperations* operations, const Animation* animation, const String& animationName, const IntSize& boxSize, int animationIndex, double timeOffset, bool isMatrixAnimation)
{
    TransformOperation::OperationType transformOp = isMatrixAnimation ? TransformOperation::MATRIX_3D : operations->operations().at(animationIndex)->getOperationType();
    bool additive = animationIndex > 0;
    bool isKeyframe = valueList.size() > 2;

    RefPtr<PlatformClutterAnimation> clutterAnimation;
    bool validMatrices = true;
    if (isKeyframe) {
        clutterAnimation = createKeyframeAnimation(animation, propertyIdToString(valueList.property()), additive);
        validMatrices = setTransformAnimationKeyframes(valueList, animation, clutterAnimation.get(), animationIndex, transformOp, isMatrixAnimation, boxSize);
    } else {
        clutterAnimation = createBasicAnimation(animation, propertyIdToString(valueList.property()), additive);
        validMatrices = setTransformAnimationEndpoints(valueList, animation, clutterAnimation.get(), animationIndex, transformOp, isMatrixAnimation, boxSize);
    }

    if (!validMatrices)
        return false;

    m_uncomittedAnimations.append(LayerPropertyAnimation(clutterAnimation, animationName, valueList.property(), animationIndex, timeOffset));
    return true;
}

bool GraphicsLayerClutter::createTransformAnimationsFromKeyframes(const KeyframeValueList& valueList, const Animation* animation, const String& animationName, double timeOffset, const IntSize& boxSize)
{
    ASSERT(valueList.property() == AnimatedPropertyWebkitTransform);

    bool hasBigRotation;
    int listIndex = validateTransformOperations(valueList, hasBigRotation);
    const TransformOperations* operations = (listIndex >= 0) ? static_cast<const TransformAnimationValue*>(valueList.at(listIndex))->value() : 0;

    // We need to fall back to software animation if we don't have setValueFunction:, and
    // we would need to animate each incoming transform function separately. This is the
    // case if we have a rotation >= 180 or we have more than one transform function.
    if ((hasBigRotation || (operations && operations->size() > 1)) && !PlatformClutterAnimation::supportsValueFunction())
        return false;

    bool validMatrices = true;

    // If function lists don't match we do a matrix animation, otherwise we do a component hardware animation.
    // Also, we can't do component animation unless we have valueFunction, so we need to do matrix animation
    // if that's not true as well.
    bool isMatrixAnimation = listIndex < 0 || !PlatformClutterAnimation::supportsValueFunction() || (operations->size() >= 2 && !PlatformClutterAnimation::supportsAdditiveValueFunction());
    int numAnimations = isMatrixAnimation ? 1 : operations->size();

    for (int animationIndex = 0; animationIndex < numAnimations; ++animationIndex) {
        if (!appendToUncommittedAnimations(valueList, operations, animation, animationName, boxSize, animationIndex, timeOffset, isMatrixAnimation)) {
            validMatrices = false;
            break;
        }
    }

    return validMatrices;
}

bool GraphicsLayerClutter::createAnimationFromKeyframes(const KeyframeValueList& valueList, const Animation* animation, const String& animationName, double timeOffset)
{
    ASSERT(valueList.property() != AnimatedPropertyWebkitTransform);

    bool isKeyframe = valueList.size() > 2;
    bool valuesOK;

    bool additive = false;
    int animationIndex = 0;

    RefPtr<PlatformClutterAnimation> clutterAnimation;

    if (isKeyframe) {
        clutterAnimation = createKeyframeAnimation(animation, propertyIdToString(valueList.property()), additive);
        valuesOK = setAnimationKeyframes(valueList, animation, clutterAnimation.get());
    } else {
        clutterAnimation = createBasicAnimation(animation, propertyIdToString(valueList.property()), additive);
        valuesOK = setAnimationEndpoints(valueList, animation, clutterAnimation.get());
    }

    if (!valuesOK)
        return false;

    m_uncomittedAnimations.append(LayerPropertyAnimation(clutterAnimation, animationName, valueList.property(), animationIndex, timeOffset));

    return true;
}

bool GraphicsLayerClutter::addAnimation(const KeyframeValueList& valueList, const IntSize& boxSize, const Animation* anim, const String& animationName, double timeOffset)
{
    ASSERT(!animationName.isEmpty());

    if (!anim || anim->isEmptyOrZeroDuration() || valueList.size() < 2)
        return false;

    // FIXME: ClutterTimeline seems to support steps timing function. So we need to improve here.
    // See http://developer.gnome.org/clutter/stable/ClutterTimeline.html#ClutterAnimationMode
    if (animationHasStepsTimingFunction(valueList, anim))
        return false;

    bool createdAnimations = false;
    if (valueList.property() == AnimatedPropertyWebkitTransform)
        createdAnimations = createTransformAnimationsFromKeyframes(valueList, anim, animationName, timeOffset, boxSize);
    else
        createdAnimations = createAnimationFromKeyframes(valueList, anim, animationName, timeOffset);

    if (createdAnimations)
        noteLayerPropertyChanged(AnimationChanged);

    return createdAnimations;
}

bool GraphicsLayerClutter::removeClutterAnimationFromLayer(AnimatedPropertyID property, const String& animationName, int index)
{
    notImplemented();
    return false;
}

void GraphicsLayerClutter::pauseClutterAnimationOnLayer(AnimatedPropertyID property, const String& animationName, int index, double timeOffset)
{
    notImplemented();
}

void GraphicsLayerClutter::setAnimationOnLayer(PlatformClutterAnimation* clutterAnim, AnimatedPropertyID property, const String& animationName, int index, double timeOffset)
{
    GraphicsLayerActor* layer = animatedLayer(property);

    if (timeOffset)
        clutterAnim->setBeginTime(g_get_real_time() - timeOffset);

    String animationID = animationIdentifier(animationName, property, index);

    PlatformClutterAnimation* existingAnimation = graphicsLayerActorGetAnimationForKey(layer, animationID);
    if (existingAnimation)
        existingAnimation->removeAnimationForKey(layer, animationID);

    clutterAnim->addAnimationForKey(layer, animationID);
}

bool GraphicsLayerClutter::setAnimationEndpoints(const KeyframeValueList& valueList, const Animation* animation, PlatformClutterAnimation* basicAnim)
{
    bool forwards = animation->directionIsForwards();

    unsigned fromIndex = !forwards;
    unsigned toIndex = forwards;

    switch (valueList.property()) {
    case AnimatedPropertyOpacity: {
        basicAnim->setFromValue(static_cast<const FloatAnimationValue*>(valueList.at(fromIndex))->value());
        basicAnim->setToValue(static_cast<const FloatAnimationValue*>(valueList.at(toIndex))->value());
        break;
    }
    default:
        ASSERT_NOT_REACHED(); // we don't animate color yet
        break;
    }

    // This codepath is used for 2-keyframe animations, so we still need to look in the start
    // for a timing function. Even in the reversing animation case, the first keyframe provides the timing function.
    const TimingFunction* timingFunction = timingFunctionForAnimationValue(valueList.at(0), animation);
    if (timingFunction)
        basicAnim->setTimingFunction(timingFunction, !forwards);

    return true;
}

bool GraphicsLayerClutter::setAnimationKeyframes(const KeyframeValueList& valueList, const Animation* animation, PlatformClutterAnimation* keyframeAnim)
{
    Vector<float> keyTimes;
    Vector<float> values;
    Vector<const TimingFunction*> timingFunctions;

    bool forwards = animation->directionIsForwards();

    for (unsigned i = 0; i < valueList.size(); ++i) {
        unsigned index = forwards ? i : (valueList.size() - i - 1);
        const AnimationValue* curValue = valueList.at(index);
        keyTimes.append(forwards ? curValue->keyTime() : (1 - curValue->keyTime()));

        switch (valueList.property()) {
        case AnimatedPropertyOpacity: {
            const FloatAnimationValue* floatValue = static_cast<const FloatAnimationValue*>(curValue);
            values.append(floatValue->value());
            break;
        }
        default:
            ASSERT_NOT_REACHED(); // we don't animate color yet
            break;
        }

        if (i < (valueList.size() - 1))
            timingFunctions.append(timingFunctionForAnimationValue(forwards ? curValue : valueList.at(index - 1), animation));
    }

    keyframeAnim->setKeyTimes(keyTimes);
    keyframeAnim->setValues(values);
    keyframeAnim->setTimingFunctions(timingFunctions, !forwards);

    return true;
}

GraphicsLayerActor* GraphicsLayerClutter::layerForSuperlayer() const
{
    return m_layer.get();
}

GraphicsLayerActor* GraphicsLayerClutter::animatedLayer(AnimatedPropertyID property) const
{
    return primaryLayer();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
