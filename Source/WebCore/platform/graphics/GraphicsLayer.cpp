/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include "GraphicsLayer.h"

#include "FloatPoint.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "LayoutRect.h"
#include "RotateTransformOperation.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace WebCore {

typedef HashMap<const GraphicsLayer*, Vector<FloatRect>> RepaintMap;
static RepaintMap& repaintRectMap()
{
    static NeverDestroyed<RepaintMap> map;
    return map;
}

void KeyframeValueList::insert(std::unique_ptr<const AnimationValue> value)
{
    for (size_t i = 0; i < m_values.size(); ++i) {
        const AnimationValue* curValue = m_values[i].get();
        if (curValue->keyTime() == value->keyTime()) {
            ASSERT_NOT_REACHED();
            // insert after
            m_values.insert(i + 1, WTFMove(value));
            return;
        }
        if (curValue->keyTime() > value->keyTime()) {
            // insert before
            m_values.insert(i, WTFMove(value));
            return;
        }
    }
    
    m_values.append(WTFMove(value));
}

#if !USE(CA)
bool GraphicsLayer::supportsLayerType(Type type)
{
    switch (type) {
    case Type::Normal:
    case Type::PageTiledBacking:
    case Type::ScrollContainer:
        return true;
    case Type::Shape:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool GraphicsLayer::supportsBackgroundColorContent()
{
#if USE(TEXTURE_MAPPER)
    return true;
#else
    return false;
#endif
}

bool GraphicsLayer::supportsSubpixelAntialiasedLayerText()
{
    return false;
}
#endif

#if !USE(COORDINATED_GRAPHICS)
bool GraphicsLayer::supportsContentsTiling()
{
    // FIXME: Enable the feature on different ports.
    return false;
}
#endif

// Singleton client used for layers on which clearClient has been called.
class EmptyGraphicsLayerClient : public GraphicsLayerClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static EmptyGraphicsLayerClient& singleton();
};

EmptyGraphicsLayerClient& EmptyGraphicsLayerClient::singleton()
{
    static NeverDestroyed<EmptyGraphicsLayerClient> client;
    return client;
}

GraphicsLayer::GraphicsLayer(Type type, GraphicsLayerClient& layerClient)
    : m_client(&layerClient)
    , m_type(type)
    , m_beingDestroyed(false)
    , m_contentsOpaque(false)
    , m_supportsSubpixelAntialiasedText(false)
    , m_preserves3D(false)
    , m_backfaceVisibility(true)
    , m_masksToBounds(false)
    , m_drawsContent(false)
    , m_contentsVisible(true)
    , m_acceleratesDrawing(false)
    , m_usesDisplayListDrawing(false)
    , m_appliesPageScale(false)
    , m_showDebugBorder(false)
    , m_showRepaintCounter(false)
    , m_isMaskLayer(false)
    , m_isTrackingDisplayListReplay(false)
    , m_userInteractionEnabled(true)
    , m_canDetachBackingStore(true)
{
#ifndef NDEBUG
    client().verifyNotPainting();
#endif
}

GraphicsLayer::~GraphicsLayer()
{
    resetTrackedRepaints();
    ASSERT(!m_parent); // willBeDestroyed should have been called already.
}

void GraphicsLayer::unparentAndClear(RefPtr<GraphicsLayer>& layer)
{
    if (layer) {
        layer->removeFromParent();
        layer->clearClient();
        layer = nullptr;
    }
}

void GraphicsLayer::clear(RefPtr<GraphicsLayer>& layer)
{
    if (layer) {
        layer->clearClient();
        layer = nullptr;
    }
}

void GraphicsLayer::willBeDestroyed()
{
    m_beingDestroyed = true;
#ifndef NDEBUG
    client().verifyNotPainting();
#endif
    if (m_replicaLayer)
        m_replicaLayer->setReplicatedLayer(nullptr);

    if (m_replicatedLayer)
        m_replicatedLayer->setReplicatedByLayer(nullptr);

    if (m_maskLayer) {
        m_maskLayer->setParent(nullptr);
        m_maskLayer->setIsMaskLayer(false);
    }

    removeAllChildren();
    removeFromParent();
}

void GraphicsLayer::clearClient()
{
    m_client = &EmptyGraphicsLayerClient::singleton();
}

void GraphicsLayer::setClient(GraphicsLayerClient& client)
{
    m_client = &client;
}

void GraphicsLayer::setParent(GraphicsLayer* layer)
{
    ASSERT(!layer || !layer->hasAncestor(this));
    m_parent = layer;
}

bool GraphicsLayer::hasAncestor(GraphicsLayer* ancestor) const
{
    for (GraphicsLayer* curr = parent(); curr; curr = curr->parent()) {
        if (curr == ancestor)
            return true;
    }
    
    return false;
}

bool GraphicsLayer::setChildren(Vector<Ref<GraphicsLayer>>&& newChildren)
{
    // If the contents of the arrays are the same, nothing to do.
    if (newChildren == m_children)
        return false;

    removeAllChildren();

    size_t listSize = newChildren.size();
    for (size_t i = 0; i < listSize; ++i)
        addChild(WTFMove(newChildren[i]));
    
    return true;
}

void GraphicsLayer::addChild(Ref<GraphicsLayer>&& childLayer)
{
    ASSERT(childLayer.ptr() != this);
    
    childLayer->removeFromParent();
    childLayer->setParent(this);
    m_children.append(WTFMove(childLayer));
}

void GraphicsLayer::addChildAtIndex(Ref<GraphicsLayer>&& childLayer, int index)
{
    ASSERT(childLayer.ptr() != this);

    childLayer->removeFromParent();
    childLayer->setParent(this);
    m_children.insert(index, WTFMove(childLayer));
}

void GraphicsLayer::addChildBelow(Ref<GraphicsLayer>&& childLayer, GraphicsLayer* sibling)
{
    ASSERT(childLayer.ptr() != this);
    childLayer->removeFromParent();

    childLayer->setParent(this);

    for (unsigned i = 0; i < m_children.size(); i++) {
        if (sibling == m_children[i].ptr()) {
            m_children.insert(i, WTFMove(childLayer));
            return;
        }
    }

    m_children.append(WTFMove(childLayer));
}

void GraphicsLayer::addChildAbove(Ref<GraphicsLayer>&& childLayer, GraphicsLayer* sibling)
{
    childLayer->removeFromParent();
    ASSERT(childLayer.ptr() != this);

    childLayer->setParent(this);

    for (unsigned i = 0; i < m_children.size(); i++) {
        if (sibling == m_children[i].ptr()) {
            m_children.insert(i + 1, WTFMove(childLayer));
            return;
        }
    }

    m_children.append(WTFMove(childLayer));
}

bool GraphicsLayer::replaceChild(GraphicsLayer* oldChild, Ref<GraphicsLayer>&& newChild)
{
    ASSERT(!newChild->parent());
    
    GraphicsLayer* rawNewChild = newChild.ptr();

    bool found = false;
    for (unsigned i = 0; i < m_children.size(); i++) {
        if (oldChild == m_children[i].ptr()) {
            m_children[i] = WTFMove(newChild);
            found = true;
            break;
        }
    }
    if (found) {
        oldChild->setParent(nullptr);

        rawNewChild->removeFromParent();
        rawNewChild->setParent(this);
        return true;
    }
    return false;
}

void GraphicsLayer::removeAllChildren()
{
    while (m_children.size()) {
        GraphicsLayer* curLayer = m_children[0].ptr();
        ASSERT(curLayer->parent());
        curLayer->removeFromParent();
        // curLayer may be destroyed here.
    }
}

void GraphicsLayer::removeFromParent()
{
    if (m_parent) {
        GraphicsLayer* parent = m_parent;
        setParent(nullptr);
        parent->m_children.removeFirstMatching([this](auto& layer) {
            return layer.ptr() == this;
        });
        // |this| may be destroyed here.
    }
}

const TransformationMatrix& GraphicsLayer::transform() const
{
    return m_transform ? *m_transform : TransformationMatrix::identity;
}

void GraphicsLayer::setTransform(const TransformationMatrix& matrix)
{
    if (m_transform)
        *m_transform = matrix;
    else
        m_transform = std::make_unique<TransformationMatrix>(matrix);
}

const TransformationMatrix& GraphicsLayer::childrenTransform() const
{
    return m_childrenTransform ? *m_childrenTransform : TransformationMatrix::identity;
}

void GraphicsLayer::setChildrenTransform(const TransformationMatrix& matrix)
{
    if (m_childrenTransform)
        *m_childrenTransform = matrix;
    else
        m_childrenTransform = std::make_unique<TransformationMatrix>(matrix);
}

void GraphicsLayer::setMaskLayer(RefPtr<GraphicsLayer>&& layer)
{
    if (layer == m_maskLayer)
        return;

    if (layer) {
        layer->removeFromParent();
        layer->setParent(this);
        layer->setIsMaskLayer(true);
    } else if (m_maskLayer) {
        m_maskLayer->setParent(nullptr);
        m_maskLayer->setIsMaskLayer(false);
    }
    
    m_maskLayer = WTFMove(layer);
}

Path GraphicsLayer::shapeLayerPath() const
{
#if USE(CA)
    return m_shapeLayerPath;
#else
    return Path();
#endif
}

void GraphicsLayer::setShapeLayerPath(const Path& path)
{
#if USE(CA)
    m_shapeLayerPath = path;
#else
    UNUSED_PARAM(path);
#endif
}

WindRule GraphicsLayer::shapeLayerWindRule() const
{
#if USE(CA)
    return m_shapeLayerWindRule;
#else
    return WindRule::NonZero;
#endif
}

void GraphicsLayer::setShapeLayerWindRule(WindRule windRule)
{
#if USE(CA)
    m_shapeLayerWindRule = windRule;
#else
    UNUSED_PARAM(windRule);
#endif
}

void GraphicsLayer::noteDeviceOrPageScaleFactorChangedIncludingDescendants()
{
    deviceOrPageScaleFactorChanged();

    if (m_maskLayer)
        m_maskLayer->deviceOrPageScaleFactorChanged();

    if (m_replicaLayer)
        m_replicaLayer->noteDeviceOrPageScaleFactorChangedIncludingDescendants();

    for (auto& layer : children())
        layer->noteDeviceOrPageScaleFactorChangedIncludingDescendants();
}

void GraphicsLayer::setIsInWindow(bool inWindow)
{
    if (TiledBacking* tiledBacking = this->tiledBacking())
        tiledBacking->setIsInWindow(inWindow);
}

void GraphicsLayer::setReplicatedByLayer(RefPtr<GraphicsLayer>&& layer)
{
    if (m_replicaLayer == layer)
        return;

    if (m_replicaLayer)
        m_replicaLayer->setReplicatedLayer(nullptr);

    if (layer)
        layer->setReplicatedLayer(this);

    m_replicaLayer = WTFMove(layer);
}

void GraphicsLayer::setOffsetFromRenderer(const FloatSize& offset, ShouldSetNeedsDisplay shouldSetNeedsDisplay)
{
    if (offset == m_offsetFromRenderer)
        return;

    m_offsetFromRenderer = offset;

    // If the compositing layer offset changes, we need to repaint.
    if (shouldSetNeedsDisplay == SetNeedsDisplay)
        setNeedsDisplay();
}

void GraphicsLayer::setScrollOffset(const ScrollOffset& offset, ShouldSetNeedsDisplay shouldSetNeedsDisplay)
{
    if (offset == m_scrollOffset)
        return;

    m_scrollOffset = offset;

    // If the compositing layer offset changes, we need to repaint.
    if (shouldSetNeedsDisplay == SetNeedsDisplay)
        setNeedsDisplay();
}

void GraphicsLayer::setSize(const FloatSize& size)
{
    if (size == m_size)
        return;
    
    m_size = size;

    if (shouldRepaintOnSizeChange())
        setNeedsDisplay();
}

void GraphicsLayer::setBackgroundColor(const Color& color)
{
    m_backgroundColor = color;
}

void GraphicsLayer::paintGraphicsLayerContents(GraphicsContext& context, const FloatRect& clip, GraphicsLayerPaintBehavior layerPaintBehavior)
{
    FloatSize offset = offsetFromRenderer() - toFloatSize(scrollOffset());
    context.translate(-offset);

    FloatRect clipRect(clip);
    clipRect.move(offset);

    client().paintContents(this, context, m_paintingPhase, clipRect, layerPaintBehavior);
}

String GraphicsLayer::animationNameForTransition(AnimatedPropertyID property)
{
    // | is not a valid identifier character in CSS, so this can never conflict with a keyframe identifier.
    StringBuilder id;
    id.appendLiteral("-|transition");
    id.appendNumber(static_cast<int>(property));
    id.append('-');
    return id.toString();
}

void GraphicsLayer::suspendAnimations(MonotonicTime)
{
}

void GraphicsLayer::resumeAnimations()
{
}

void GraphicsLayer::getDebugBorderInfo(Color& color, float& width) const
{
    width = 2;

    if (needsBackdrop()) {
        color = Color(255, 0, 255, 128); // has backdrop: magenta
        width = 12;
        return;
    }
    
    if (drawsContent()) {
        if (tiledBacking()) {
            color = Color(255, 128, 0, 128); // tiled layer: orange
            return;
        }

        color = Color(0, 128, 32, 128); // normal layer: green
        return;
    }

    if (usesContentsLayer()) {
        color = Color(0, 64, 128, 150); // non-painting layer with contents: blue
        width = 8;
        return;
    }
    
    if (masksToBounds()) {
        color = Color(128, 255, 255, 48); // masking layer: pale blue
        width = 16;
        return;
    }

    color = Color(255, 255, 0, 192); // container: yellow
}

void GraphicsLayer::updateDebugIndicators()
{
    if (!isShowingDebugBorder())
        return;

    Color borderColor;
    float width = 0;
    getDebugBorderInfo(borderColor, width);
    setDebugBorder(borderColor, width);
}

void GraphicsLayer::setZPosition(float position)
{
    m_zPosition = position;
}

float GraphicsLayer::accumulatedOpacity() const
{
    if (!preserves3D())
        return 1;
        
    return m_opacity * (parent() ? parent()->accumulatedOpacity() : 1);
}

void GraphicsLayer::distributeOpacity(float accumulatedOpacity)
{
    // If this is a transform layer we need to distribute our opacity to all our children
    
    // Incoming accumulatedOpacity is the contribution from our parent(s). We mutiply this by our own
    // opacity to get the total contribution
    accumulatedOpacity *= m_opacity;
    
    setOpacityInternal(accumulatedOpacity);
    
    if (preserves3D()) {
        for (auto& layer : children())
            layer->distributeOpacity(accumulatedOpacity);
    }
}

static inline const FilterOperations& filterOperationsAt(const KeyframeValueList& valueList, size_t index)
{
    return static_cast<const FilterAnimationValue&>(valueList.at(index)).value();
}

int GraphicsLayer::validateFilterOperations(const KeyframeValueList& valueList)
{
#if ENABLE(FILTERS_LEVEL_2)
    ASSERT(valueList.property() == AnimatedPropertyFilter || valueList.property() == AnimatedPropertyWebkitBackdropFilter);
#else
    ASSERT(valueList.property() == AnimatedPropertyFilter);
#endif

    if (valueList.size() < 2)
        return -1;

    // Empty filters match anything, so find the first non-empty entry as the reference
    size_t firstIndex = 0;
    for ( ; firstIndex < valueList.size(); ++firstIndex) {
        if (!filterOperationsAt(valueList, firstIndex).operations().isEmpty())
            break;
    }

    if (firstIndex >= valueList.size())
        return -1;

    const FilterOperations& firstVal = filterOperationsAt(valueList, firstIndex);
    
    for (size_t i = firstIndex + 1; i < valueList.size(); ++i) {
        const FilterOperations& val = filterOperationsAt(valueList, i);
        
        // An emtpy filter list matches anything.
        if (val.operations().isEmpty())
            continue;
        
        if (!firstVal.operationsMatch(val))
            return -1;
    }
    
    return firstIndex;
}

// An "invalid" list is one whose functions don't match, and therefore has to be animated as a Matrix
// The hasBigRotation flag will always return false if isValid is false. Otherwise hasBigRotation is 
// true if the rotation between any two keyframes is >= 180 degrees.

static inline const TransformOperations& operationsAt(const KeyframeValueList& valueList, size_t index)
{
    return static_cast<const TransformAnimationValue&>(valueList.at(index)).value();
}

int GraphicsLayer::validateTransformOperations(const KeyframeValueList& valueList, bool& hasBigRotation)
{
    ASSERT(valueList.property() == AnimatedPropertyTransform);

    hasBigRotation = false;
    
    if (valueList.size() < 2)
        return -1;
    
    // Empty transforms match anything, so find the first non-empty entry as the reference.
    size_t firstIndex = 0;
    for ( ; firstIndex < valueList.size(); ++firstIndex) {
        if (!operationsAt(valueList, firstIndex).operations().isEmpty())
            break;
    }
    
    if (firstIndex >= valueList.size())
        return -1;
        
    const TransformOperations& firstVal = operationsAt(valueList, firstIndex);
    
    // See if the keyframes are valid.
    for (size_t i = firstIndex + 1; i < valueList.size(); ++i) {
        const TransformOperations& val = operationsAt(valueList, i);
        
        // An empty transform list matches anything.
        if (val.operations().isEmpty())
            continue;
            
        if (!firstVal.operationsMatch(val))
            return -1;
    }

    // Keyframes are valid, check for big rotations.    
    double lastRotationAngle = 0.0;
    double maxRotationAngle = -1.0;
        
    for (size_t j = 0; j < firstVal.operations().size(); ++j) {
        TransformOperation::OperationType type = firstVal.operations().at(j)->type();
        
        // if this is a rotation entry, we need to see if any angle differences are >= 180 deg
        if (type == TransformOperation::ROTATE_X ||
            type == TransformOperation::ROTATE_Y ||
            type == TransformOperation::ROTATE_Z ||
            type == TransformOperation::ROTATE_3D) {
            lastRotationAngle = downcast<RotateTransformOperation>(*firstVal.operations().at(j)).angle();
            
            if (maxRotationAngle < 0)
                maxRotationAngle = fabs(lastRotationAngle);
            
            for (size_t i = firstIndex + 1; i < valueList.size(); ++i) {
                const TransformOperations& val = operationsAt(valueList, i);
                double rotationAngle = val.operations().isEmpty() ? 0 : downcast<RotateTransformOperation>(*val.operations().at(j)).angle();
                double diffAngle = fabs(rotationAngle - lastRotationAngle);
                if (diffAngle > maxRotationAngle)
                    maxRotationAngle = diffAngle;
                lastRotationAngle = rotationAngle;
            }
        }
    }
    
    hasBigRotation = maxRotationAngle >= 180.0;
    
    return firstIndex;
}

double GraphicsLayer::backingStoreMemoryEstimate() const
{
    if (!drawsContent())
        return 0;
    
    // Effects of page and device scale are ignored; subclasses should override to take these into account.
    return static_cast<double>(4 * size().width()) * size().height();
}

void GraphicsLayer::resetTrackedRepaints()
{
    repaintRectMap().remove(this);
}

void GraphicsLayer::addRepaintRect(const FloatRect& repaintRect)
{
    if (!client().isTrackingRepaints())
        return;

    FloatRect largestRepaintRect(FloatPoint(), m_size);
    largestRepaintRect.intersect(repaintRect);
    RepaintMap::iterator repaintIt = repaintRectMap().find(this);
    if (repaintIt == repaintRectMap().end()) {
        Vector<FloatRect> repaintRects;
        repaintRects.append(largestRepaintRect);
        repaintRectMap().set(this, repaintRects);
    } else {
        Vector<FloatRect>& repaintRects = repaintIt->value;
        repaintRects.append(largestRepaintRect);
    }
}

void GraphicsLayer::traverse(GraphicsLayer& layer, const WTF::Function<void (GraphicsLayer&)>& traversalFunc)
{
    traversalFunc(layer);

    for (auto& childLayer : layer.children())
        traverse(childLayer.get(), traversalFunc);

    if (auto* replicaLayer = layer.replicaLayer())
        traverse(*replicaLayer, traversalFunc);

    if (auto* maskLayer = layer.maskLayer())
        traverse(*maskLayer, traversalFunc);
}

GraphicsLayer::EmbeddedViewID GraphicsLayer::nextEmbeddedViewID()
{
    static GraphicsLayer::EmbeddedViewID nextEmbeddedViewID;
    return ++nextEmbeddedViewID;
}

void GraphicsLayer::dumpLayer(TextStream& ts, LayerTreeAsTextBehavior behavior) const
{
    ts << indent << "(" << "GraphicsLayer";

    if (behavior & LayerTreeAsTextDebug) {
        ts << " " << static_cast<void*>(const_cast<GraphicsLayer*>(this));
        ts << " \"" << m_name << "\"";
    }

    ts << "\n";
    dumpProperties(ts, behavior);
    ts << indent << ")\n";
}

static void dumpChildren(TextStream& ts, const Vector<Ref<GraphicsLayer>>& children, unsigned& totalChildCount, LayerTreeAsTextBehavior behavior)
{
    totalChildCount += children.size();
    for (auto& child : children) {
        if ((behavior & LayerTreeAsTextDebug) || !child->client().shouldSkipLayerInDump(child.ptr(), behavior)) {
            TextStream::IndentScope indentScope(ts);
            child->dumpLayer(ts, behavior);
            continue;
        }

        totalChildCount--;
        dumpChildren(ts, child->children(), totalChildCount, behavior);
    }
}

void GraphicsLayer::dumpProperties(TextStream& ts, LayerTreeAsTextBehavior behavior) const
{
    TextStream::IndentScope indentScope(ts);
    if (!m_offsetFromRenderer.isZero())
        ts << indent << "(offsetFromRenderer " << m_offsetFromRenderer << ")\n";

    if (!m_scrollOffset.isZero())
        ts << indent << "(scrollOffset " << m_scrollOffset << ")\n";

    if (m_position != FloatPoint())
        ts << indent << "(position " << m_position.x() << " " << m_position.y() << ")\n";

    if (m_approximatePosition)
        ts << indent << "(approximate position " << m_approximatePosition.value().x() << " " << m_approximatePosition.value().y() << ")\n";

    if (m_boundsOrigin != FloatPoint())
        ts << indent << "(bounds origin " << m_boundsOrigin.x() << " " << m_boundsOrigin.y() << ")\n";

    if (m_anchorPoint != FloatPoint3D(0.5f, 0.5f, 0)) {
        ts << indent << "(anchor " << m_anchorPoint.x() << " " << m_anchorPoint.y();
        if (m_anchorPoint.z())
            ts << " " << m_anchorPoint.z();
        ts << ")\n";
    }

    if (m_size != IntSize())
        ts << indent << "(bounds " << m_size.width() << " " << m_size.height() << ")\n";

    if (m_opacity != 1)
        ts << indent << "(opacity " << m_opacity << ")\n";

#if ENABLE(CSS_COMPOSITING)
    if (m_blendMode != BlendMode::Normal)
        ts << indent << "(blendMode " << compositeOperatorName(CompositeSourceOver, m_blendMode) << ")\n";
#endif

    if (type() == Type::Normal && tiledBacking())
        ts << indent << "(usingTiledLayer 1)\n";

    bool needsIOSDumpRenderTreeMainFrameRenderViewLayerIsAlwaysOpaqueHack = client().needsIOSDumpRenderTreeMainFrameRenderViewLayerIsAlwaysOpaqueHack(*this);
    if (m_contentsOpaque || needsIOSDumpRenderTreeMainFrameRenderViewLayerIsAlwaysOpaqueHack)
        ts << indent << "(contentsOpaque " << (m_contentsOpaque || needsIOSDumpRenderTreeMainFrameRenderViewLayerIsAlwaysOpaqueHack) << ")\n";

    if (m_supportsSubpixelAntialiasedText)
        ts << indent << "(supports subpixel antialiased text " << m_supportsSubpixelAntialiasedText << ")\n";

    if (m_preserves3D)
        ts << indent << "(preserves3D " << m_preserves3D << ")\n";

    if (m_drawsContent && client().shouldDumpPropertyForLayer(this, "drawsContent"))
        ts << indent << "(drawsContent " << m_drawsContent << ")\n";

    if (!m_contentsVisible)
        ts << indent << "(contentsVisible " << m_contentsVisible << ")\n";

    if (!m_backfaceVisibility)
        ts << indent << "(backfaceVisibility " << (m_backfaceVisibility ? "visible" : "hidden") << ")\n";

    if (behavior & LayerTreeAsTextDebug) {
        ts << indent << "(primary-layer-id " << primaryLayerID() << ")\n";
        ts << indent << "(client " << static_cast<void*>(m_client) << ")\n";
    }

    if (m_backgroundColor.isValid() && client().shouldDumpPropertyForLayer(this, "backgroundColor"))
        ts << indent << "(backgroundColor " << m_backgroundColor.nameForRenderTreeAsText() << ")\n";

    if (behavior & LayerTreeAsTextIncludeAcceleratesDrawing && m_acceleratesDrawing)
        ts << indent << "(acceleratesDrawing " << m_acceleratesDrawing << ")\n";

    if (behavior & LayerTreeAsTextIncludeBackingStoreAttached)
        ts << indent << "(backingStoreAttached " << backingStoreAttachedForTesting() << ")\n";

    if (m_transform && !m_transform->isIdentity()) {
        ts << indent << "(transform ";
        ts << "[" << m_transform->m11() << " " << m_transform->m12() << " " << m_transform->m13() << " " << m_transform->m14() << "] ";
        ts << "[" << m_transform->m21() << " " << m_transform->m22() << " " << m_transform->m23() << " " << m_transform->m24() << "] ";
        ts << "[" << m_transform->m31() << " " << m_transform->m32() << " " << m_transform->m33() << " " << m_transform->m34() << "] ";
        ts << "[" << m_transform->m41() << " " << m_transform->m42() << " " << m_transform->m43() << " " << m_transform->m44() << "])\n";
    }

    // Avoid dumping the sublayer transform on the root layer, because it's used for geometry flipping, whose behavior
    // differs between platforms.
    if (parent() && m_childrenTransform && !m_childrenTransform->isIdentity()) {
        ts << indent << "(childrenTransform ";
        ts << "[" << m_childrenTransform->m11() << " " << m_childrenTransform->m12() << " " << m_childrenTransform->m13() << " " << m_childrenTransform->m14() << "] ";
        ts << "[" << m_childrenTransform->m21() << " " << m_childrenTransform->m22() << " " << m_childrenTransform->m23() << " " << m_childrenTransform->m24() << "] ";
        ts << "[" << m_childrenTransform->m31() << " " << m_childrenTransform->m32() << " " << m_childrenTransform->m33() << " " << m_childrenTransform->m34() << "] ";
        ts << "[" << m_childrenTransform->m41() << " " << m_childrenTransform->m42() << " " << m_childrenTransform->m43() << " " << m_childrenTransform->m44() << "])\n";
    }

    if (m_maskLayer) {
        ts << indent << "(mask layer";
        if (behavior & LayerTreeAsTextDebug)
            ts << " " << m_maskLayer;
        ts << ")\n";

        TextStream::IndentScope indentScope(ts);
        m_maskLayer->dumpLayer(ts, behavior);
    }

    if (m_replicaLayer) {
        ts << indent << "(replica layer";
        if (behavior & LayerTreeAsTextDebug)
            ts << " " << m_replicaLayer;
        ts << ")\n";

        TextStream::IndentScope indentScope(ts);
        m_replicaLayer->dumpLayer(ts, behavior);
    }

    if (m_replicatedLayer) {
        ts << indent << "(replicated layer";
        if (behavior & LayerTreeAsTextDebug)
            ts << " " << m_replicatedLayer;
        ts << ")\n";
    }

    if (behavior & LayerTreeAsTextIncludeRepaintRects && repaintRectMap().contains(this) && !repaintRectMap().get(this).isEmpty() && client().shouldDumpPropertyForLayer(this, "repaintRects")) {
        ts << indent << "(repaint rects\n";
        for (size_t i = 0; i < repaintRectMap().get(this).size(); ++i) {
            if (repaintRectMap().get(this)[i].isEmpty())
                continue;

            TextStream::IndentScope indentScope(ts);
            ts << indent << "(rect ";
            ts << repaintRectMap().get(this)[i].x() << " ";
            ts << repaintRectMap().get(this)[i].y() << " ";
            ts << repaintRectMap().get(this)[i].width() << " ";
            ts << repaintRectMap().get(this)[i].height();
            ts << ")\n";
        }
        ts << indent << ")\n";
    }

    if (behavior & LayerTreeAsTextIncludePaintingPhases && paintingPhase()) {
        ts << indent << "(paintingPhases\n";
        TextStream::IndentScope indentScope(ts);
        if (paintingPhase() & GraphicsLayerPaintBackground)
            ts << indent << "GraphicsLayerPaintBackground\n";

        if (paintingPhase() & GraphicsLayerPaintForeground)
            ts << indent << "GraphicsLayerPaintForeground\n";

        if (paintingPhase() & GraphicsLayerPaintMask)
            ts << indent << "GraphicsLayerPaintMask\n";

        if (paintingPhase() & GraphicsLayerPaintChildClippingMask)
            ts << indent << "GraphicsLayerPaintChildClippingMask\n";

        if (paintingPhase() & GraphicsLayerPaintOverflowContents)
            ts << indent << "GraphicsLayerPaintOverflowContents\n";

        if (paintingPhase() & GraphicsLayerPaintCompositedScroll)
            ts << indent << "GraphicsLayerPaintCompositedScroll\n";

        ts << indent << ")\n";
    }

    dumpAdditionalProperties(ts, behavior);
    
    if (m_children.size()) {
        TextStream childrenStream;
        
        childrenStream.increaseIndent(ts.indent());
        unsigned totalChildCount = 0;
        dumpChildren(childrenStream, m_children, totalChildCount, behavior);

        if (totalChildCount) {
            ts << indent << "(children " << totalChildCount << "\n";
            ts << childrenStream.release();
            ts << indent << ")\n";
        }
    }
}

TextStream& operator<<(TextStream& ts, const Vector<GraphicsLayer::PlatformLayerID>& layers)
{
    for (size_t i = 0; i < layers.size(); ++i) {
        if (i)
            ts << " ";
        ts << layers[i];
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, const WebCore::GraphicsLayer::CustomAppearance& customAppearance)
{
    switch (customAppearance) {
    case GraphicsLayer::CustomAppearance::None: ts << "none"; break;
    case GraphicsLayer::CustomAppearance::ScrollingOverhang: ts << "scrolling-overhang"; break;
    case GraphicsLayer::CustomAppearance::ScrollingShadow: ts << "scrolling-shadow"; break;
    case GraphicsLayer::CustomAppearance::LightBackdrop: ts << "light-backdrop"; break;
    case GraphicsLayer::CustomAppearance::DarkBackdrop: ts << "dark-backdrop"; break;
    }
    return ts;
}

String GraphicsLayer::layerTreeAsText(LayerTreeAsTextBehavior behavior) const
{
    TextStream ts(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);

    dumpLayer(ts, behavior);
    return ts.release();
}

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
void showGraphicsLayerTree(const WebCore::GraphicsLayer* layer)
{
    if (!layer)
        return;

    String output = layer->layerTreeAsText(WebCore::LayerTreeAsTextShowAll);
    WTFLogAlways("%s\n", output.utf8().data());
}
#endif
