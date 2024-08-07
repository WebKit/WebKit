/*
 * Copyright (C) 2009-2023 Apple Inc. All rights reserved.
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

#include "ColorSerialization.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "LayoutRect.h"
#include "MediaPlayerEnums.h"
#include "RotateTransformOperation.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
#include "AcceleratedEffectStack.h"
#endif

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace WebCore {

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
String acceleratedEffectPropertyIDAsString(AcceleratedEffectProperty property)
{
    switch (property) {
    case AcceleratedEffectProperty::Opacity:
        return "opacity"_s;
    case AcceleratedEffectProperty::Transform:
        return "transform"_s;
    case AcceleratedEffectProperty::Translate:
        return "translate"_s;
    case AcceleratedEffectProperty::Rotate:
        return "rotate"_s;
    case AcceleratedEffectProperty::Scale:
        return "scale"_s;
    case AcceleratedEffectProperty::OffsetPath:
        return "offset-path"_s;
    case AcceleratedEffectProperty::OffsetDistance:
        return "offset-distance"_s;
    case AcceleratedEffectProperty::OffsetPosition:
        return "offset-position"_s;
    case AcceleratedEffectProperty::OffsetAnchor:
        return "offset-anchor"_s;
    case AcceleratedEffectProperty::OffsetRotate:
        return "offset-rotate"_s;
    case AcceleratedEffectProperty::Filter:
        return "filter"_s;
    case AcceleratedEffectProperty::BackdropFilter:
        return "backdrop-filter"_s;
    default:
        ASSERT_NOT_REACHED();
        return "invalid"_s;
    }
    ASSERT_NOT_REACHED();
    return ""_s;
}
#endif

String animatedPropertyIDAsString(AnimatedProperty property)
{
    switch (property) {
    case AnimatedProperty::Translate:
    case AnimatedProperty::Scale:
    case AnimatedProperty::Rotate:
    case AnimatedProperty::Transform:
        return "transform"_s;
    case AnimatedProperty::Opacity:
        return "opacity"_s;
    case AnimatedProperty::BackgroundColor:
        return "background-color"_s;
    case AnimatedProperty::Filter:
        return "filter"_s;
    case AnimatedProperty::WebkitBackdropFilter:
        return "backdrop-filter"_s;
    case AnimatedProperty::Invalid:
        return "invalid"_s;
    }
    ASSERT_NOT_REACHED();
    return ""_s;
}

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
    case Type::Structural:
    case Type::PageTiledBacking:
    case Type::ScrollContainer:
    case Type::ScrolledContents:
    case Type::TiledBacking:
        return true;
    case Type::Shape:
        return false;
    }
    ASSERT_NOT_REACHED();
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
class EmptyGraphicsLayerClient final : public GraphicsLayerClient {
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
    , m_preserves3D(type == Type::Structural)
    , m_backfaceVisibility(true)
    , m_masksToBounds(false)
    , m_drawsContent(false)
    , m_contentsVisible(true)
    , m_contentsRectClipsDescendants(false)
    , m_acceleratesDrawing(false)
    , m_usesDisplayListDrawing(false)
    , m_allowsTiling(true)
    , m_appliesPageScale(false)
    , m_appliesDeviceScale(true)
    , m_showDebugBorder(false)
    , m_showRepaintCounter(false)
    , m_isMaskLayer(false)
    , m_isBackdropRoot(false)
    , m_isTrackingDisplayListReplay(false)
    , m_userInteractionEnabled(true)
    , m_canDetachBackingStore(true)
    , m_shouldPaintUsingCompositeCopy(false)
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    , m_isSeparated(false)
#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
    , m_isSeparatedPortal(false)
    , m_isDescendentOfSeparatedPortal(false)
#endif
#endif
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
    removeFromParentInternal();
}

void GraphicsLayer::clearClient()
{
    m_client = &EmptyGraphicsLayerClient::singleton();
}

String GraphicsLayer::debugName() const
{
    return name();
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
    if (m_children.isEmpty())
        return;

    willModifyChildren();

    for (auto& child : m_children)
        child->setParent(nullptr);

    m_children.clear();
}

void GraphicsLayer::removeFromParentInternal()
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

void GraphicsLayer::setPreserves3D(bool b)
{
    ASSERT_IMPLIES(m_type == Type::Structural, b);
    m_preserves3D = b;
}

void GraphicsLayer::setMasksToBounds(bool b)
{
    ASSERT_IMPLIES(m_type == Type::Structural, false);
    m_masksToBounds = b;
}

void GraphicsLayer::setDrawsContent(bool b)
{
    ASSERT_IMPLIES(m_type == Type::Structural, false);
    m_drawsContent = b;
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
        m_transform = makeUnique<TransformationMatrix>(matrix);
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
        m_childrenTransform = makeUnique<TransformationMatrix>(matrix);
}

bool GraphicsLayer::setFilters(const FilterOperations& filters)
{
    ASSERT(m_type != Type::Structural);
    m_filters = filters;
    return true;
}

void GraphicsLayer::setOpacity(float opacity)
{
    ASSERT(m_type != Type::Structural);
    m_opacity = opacity;
}

void GraphicsLayer::removeFromParent()
{
    if (m_parent)
        m_parent->willModifyChildren();

    // removeFromParentInternal is nonvirtual, for use in willBeDestroyed,
    // which is called from destructors.
    removeFromParentInternal();
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

MediaPlayerVideoGravity GraphicsLayer::videoGravity() const
{
#if USE(CA)
    return m_videoGravity;
#else
    return MediaPlayerVideoGravity::ResizeAspect;
#endif
}

void GraphicsLayer::setVideoGravity(MediaPlayerVideoGravity gravity)
{
#if USE(CA)
    m_videoGravity = gravity;
#else
    UNUSED_PARAM(gravity);
#endif
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

void GraphicsLayer::setEventRegion(EventRegion&& eventRegion)
{
    m_eventRegion = WTFMove(eventRegion);
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
    if (auto* tiledBacking = this->tiledBacking())
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
    ASSERT(m_type != Type::Structural);
    m_backgroundColor = color;
}

void GraphicsLayer::setPaintingPhase(OptionSet<GraphicsLayerPaintingPhase> phase)
{
    if (phase == m_paintingPhase)
        return;

    setNeedsDisplay();
    m_paintingPhase = phase;
}

void GraphicsLayer::paintGraphicsLayerContents(GraphicsContext& context, const FloatRect& clip, OptionSet<GraphicsLayerPaintBehavior> layerPaintBehavior)
{
    auto offset = offsetFromRenderer() - toFloatSize(scrollOffset());
    auto clipRect = clip;

    if (!offset.isZero()) {
        context.translate(-offset);
        clipRect.move(offset);
    }

    client().paintContents(this, context, clipRect, layerPaintBehavior);
}

FloatRect GraphicsLayer::adjustCoverageRectForMovement(const FloatRect& coverageRect, const FloatRect& previousVisibleRect, const FloatRect& currentVisibleRect)
{
    // If the old visible rect is empty, we have no information about how the visible area is changing
    // (maybe the layer was just created), so don't attempt to expand. Also don't attempt to expand if the rects don't overlap.
    if (previousVisibleRect.isEmpty() || !currentVisibleRect.intersects(previousVisibleRect))
        return unionRect(coverageRect, currentVisibleRect);

    const float paddingMultiplier = 2;

    float leftEdgeDelta = paddingMultiplier * (currentVisibleRect.x() - previousVisibleRect.x());
    float rightEdgeDelta = paddingMultiplier * (currentVisibleRect.maxX() - previousVisibleRect.maxX());

    float topEdgeDelta = paddingMultiplier * (currentVisibleRect.y() - previousVisibleRect.y());
    float bottomEdgeDelta = paddingMultiplier * (currentVisibleRect.maxY() - previousVisibleRect.maxY());
    
    FloatRect expandedRect = currentVisibleRect;

    // More exposed on left side.
    if (leftEdgeDelta < 0) {
        float newLeft = expandedRect.x() + leftEdgeDelta;
        // Pad to the left, but don't reduce padding that's already in the backing store (since we're still exposing to the left).
        if (newLeft < previousVisibleRect.x())
            expandedRect.shiftXEdgeTo(newLeft);
        else
            expandedRect.shiftXEdgeTo(previousVisibleRect.x());
    }

    // More exposed on right.
    if (rightEdgeDelta > 0) {
        float newRight = expandedRect.maxX() + rightEdgeDelta;
        // Pad to the right, but don't reduce padding that's already in the backing store (since we're still exposing to the right).
        if (newRight > previousVisibleRect.maxX())
            expandedRect.setWidth(newRight - expandedRect.x());
        else
            expandedRect.setWidth(previousVisibleRect.maxX() - expandedRect.x());
    }

    // More exposed at top.
    if (topEdgeDelta < 0) {
        float newTop = expandedRect.y() + topEdgeDelta;
        if (newTop < previousVisibleRect.y())
            expandedRect.shiftYEdgeTo(newTop);
        else
            expandedRect.shiftYEdgeTo(previousVisibleRect.y());
    }

    // More exposed on bottom.
    if (bottomEdgeDelta > 0) {
        float newBottom = expandedRect.maxY() + bottomEdgeDelta;
        if (newBottom > previousVisibleRect.maxY())
            expandedRect.setHeight(newBottom - expandedRect.y());
        else
            expandedRect.setHeight(previousVisibleRect.maxY() - expandedRect.y());
    }
    
    return unionRect(coverageRect, expandedRect);
}

String GraphicsLayer::animationNameForTransition(AnimatedProperty property)
{
    // | is not a valid identifier character in CSS, so this can never conflict with a keyframe identifier.
    return makeString("-|transition"_s, static_cast<int>(property), '-');
}

void GraphicsLayer::suspendAnimations(MonotonicTime)
{
}

void GraphicsLayer::resumeAnimations()
{
}

void GraphicsLayer::setContentsDisplayDelegate(RefPtr<GraphicsLayerContentsDisplayDelegate>&&, ContentsLayerPurpose)
{
}

RefPtr<GraphicsLayerAsyncContentsDisplayDelegate> GraphicsLayer::createAsyncContentsDisplayDelegate(GraphicsLayerAsyncContentsDisplayDelegate*)
{
    return nullptr;
}

void GraphicsLayer::getDebugBorderInfo(Color& color, float& width) const
{
    width = 2;

    if (needsBackdrop()) {
        color = Color::magenta.colorWithAlphaByte(128); // has backdrop: magenta
        width = 12;
        return;
    }
    
    if (drawsContent()) {
        if (tiledBacking()) {
            color = Color::orange.colorWithAlphaByte(128); // tiled layer: orange
            return;
        }

        color = SRGBA<uint8_t> { 0, 128, 32, 128 }; // normal layer: green
        return;
    }

    if (usesContentsLayer()) {
        color = SRGBA<uint8_t> { 0, 64, 128, 150 }; // non-painting layer with contents: blue
        width = 8;
        return;
    }
    
    if (masksToBounds()) {
        color = SRGBA<uint8_t> { 128, 255, 255, 48 }; // masking layer: pale blue
        width = 16;
        return;
    }

    color = Color::yellow.colorWithAlphaByte(192); // container: yellow
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

static inline const FilterOperations& filterOperationsAt(const KeyframeValueList& valueList, size_t index)
{
    return static_cast<const FilterAnimationValue&>(valueList.at(index)).value();
}

int GraphicsLayer::validateFilterOperations(const KeyframeValueList& valueList)
{
    ASSERT(valueList.property() == AnimatedProperty::Filter || valueList.property() == AnimatedProperty::WebkitBackdropFilter);

    if (valueList.size() < 2)
        return -1;

    // Empty filters match anything, so find the first non-empty entry as the reference
    size_t firstIndex = 0;
    for ( ; firstIndex < valueList.size(); ++firstIndex) {
        if (!filterOperationsAt(valueList, firstIndex).isEmpty())
            break;
    }

    if (firstIndex >= valueList.size())
        return -1;

    const FilterOperations& firstVal = filterOperationsAt(valueList, firstIndex);
    
    for (size_t i = firstIndex + 1; i < valueList.size(); ++i) {
        const FilterOperations& val = filterOperationsAt(valueList, i);
        
        // An empty filter list matches anything.
        if (val.isEmpty())
            continue;
        
        if (!firstVal.operationsMatch(val))
            return -1;
    }
    
    return firstIndex;
}

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
void GraphicsLayer::setAcceleratedEffectsAndBaseValues(AcceleratedEffects&& effects, AcceleratedEffectValues&& baseValues)
{
    if (effects.isEmpty()) {
        m_effectStack = nullptr;
        return;
    }

    if (!m_effectStack)
        m_effectStack = AcceleratedEffectStack::create();

    m_effectStack->setEffects(WTFMove(effects));
    m_effectStack->setBaseValues(WTFMove(baseValues));
}
#endif

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

    repaintRectMap().add(this, Vector<FloatRect>()).iterator->value.append(WTFMove(largestRepaintRect));
}

void GraphicsLayer::traverse(GraphicsLayer& layer, const Function<void(GraphicsLayer&)>& traversalFunc)
{
    traversalFunc(layer);

    for (auto& childLayer : layer.children())
        traverse(childLayer.get(), traversalFunc);

    if (auto* replicaLayer = layer.replicaLayer())
        traverse(*replicaLayer, traversalFunc);

    if (auto* maskLayer = layer.maskLayer())
        traverse(*maskLayer, traversalFunc);
}

void GraphicsLayer::setTileCoverage(TileCoverage coverage)
{
    if (auto* backing = tiledBacking())
        backing->setTileCoverage(coverage);
}

void GraphicsLayer::dumpLayer(TextStream& ts, OptionSet<LayerTreeAsTextOptions> options) const
{
    ts << indent << "(" << "GraphicsLayer";

    if (options & LayerTreeAsTextOptions::Debug) {
        ts << " " << static_cast<void*>(const_cast<GraphicsLayer*>(this));
        ts << " \"" << m_name << "\"";
    }

    ts << "\n";
    dumpProperties(ts, options);
    ts << indent << ")\n";
}

static void dumpChildren(TextStream& ts, const Vector<Ref<GraphicsLayer>>& children, unsigned& totalChildCount, OptionSet<LayerTreeAsTextOptions> options)
{
    totalChildCount += children.size();
    for (auto& child : children) {
        if ((options & LayerTreeAsTextOptions::Debug) || !child->client().shouldSkipLayerInDump(child.ptr(), options)) {
            TextStream::IndentScope indentScope(ts);
            child->dumpLayer(ts, options);
            continue;
        }

        totalChildCount--;
        dumpChildren(ts, child->children(), totalChildCount, options);
    }
}

void GraphicsLayer::dumpProperties(TextStream& ts, OptionSet<LayerTreeAsTextOptions> options) const
{
    TextStream::IndentScope indentScope(ts);
    if (!m_offsetFromRenderer.isZero())
        ts << indent << "(offsetFromRenderer "_s << m_offsetFromRenderer << ")\n"_s;

    if (!m_scrollOffset.isZero())
        ts << indent << "(scrollOffset "_s << m_scrollOffset << ")\n"_s;

    if (m_position != FloatPoint())
        ts << indent << "(position "_s << m_position.x() << ' ' << m_position.y() << ")\n"_s;

    if (m_approximatePosition)
        ts << indent << "(approximate position "_s << m_approximatePosition.value().x() << ' ' << m_approximatePosition.value().y() << ")\n"_s;

    if (m_boundsOrigin != FloatPoint())
        ts << indent << "(bounds origin "_s << m_boundsOrigin.x() << ' ' << m_boundsOrigin.y() << ")\n"_s;

    if (client().shouldDumpPropertyForLayer(this, "anchorPoint"_s, options)) {
        ts << indent << "(anchor "_s << m_anchorPoint.x() << ' ' << m_anchorPoint.y();
        if (m_anchorPoint.z())
            ts << " " << m_anchorPoint.z();
        ts << ")\n"_s;
    }

    if (m_size != IntSize())
        ts << indent << "(bounds "_s << m_size.width() << ' ' << m_size.height() << ")\n"_s;

    if (m_opacity != 1)
        ts << indent << "(opacity "_s << m_opacity << ")\n"_s;

    if (m_blendMode != BlendMode::Normal)
        ts << indent << "(blendMode "_s << compositeOperatorName(CompositeOperator::SourceOver, m_blendMode) << ")\n"_s;

    if (type() == Type::Normal && tiledBacking())
        ts << indent << "(usingTiledLayer 1)\n"_s;

    bool needsIOSDumpRenderTreeMainFrameRenderViewLayerIsAlwaysOpaqueHack = client().needsIOSDumpRenderTreeMainFrameRenderViewLayerIsAlwaysOpaqueHack(*this);
    if (m_contentsOpaque || needsIOSDumpRenderTreeMainFrameRenderViewLayerIsAlwaysOpaqueHack)
        ts << indent << "(contentsOpaque "_s << (m_contentsOpaque || needsIOSDumpRenderTreeMainFrameRenderViewLayerIsAlwaysOpaqueHack) << ")\n"_s;

    if (m_masksToBounds && options & LayerTreeAsTextOptions::IncludeClipping)
        ts << indent << "(clips "_s << m_masksToBounds << ")\n"_s;

    if (m_preserves3D)
        ts << indent << "(preserves3D "_s << m_preserves3D << ")\n"_s;

    if (m_drawsContent && client().shouldDumpPropertyForLayer(this, "drawsContent"_s, options))
        ts << indent << "(drawsContent "_s << m_drawsContent << ")\n"_s;

    if (!m_contentsVisible)
        ts << indent << "(contentsVisible "_s << m_contentsVisible << ")\n"_s;

    if (!m_backfaceVisibility)
        ts << indent << "(backfaceVisibility "_s << (m_backfaceVisibility ? "visible"_s : "hidden"_s) << ")\n"_s;

    if (m_isBackdropRoot)
        ts << indent << "(backdropRoot "_s << m_isBackdropRoot << ")\n"_s;

    if (options & LayerTreeAsTextOptions::Debug)
        ts << indent << "(primary-layer-id "_s << primaryLayerID() << ")\n"_s;

    if (m_backgroundColor.isValid() && client().shouldDumpPropertyForLayer(this, "backgroundColor"_s, options))
        ts << indent << "(backgroundColor "_s << serializationForRenderTreeAsText(m_backgroundColor) << ")\n"_s;

    if (options & LayerTreeAsTextOptions::IncludeAcceleratesDrawing && m_acceleratesDrawing)
        ts << indent << "(acceleratesDrawing "_s << m_acceleratesDrawing << ")\n"_s;

    if (options & LayerTreeAsTextOptions::IncludeBackingStoreAttached)
        ts << indent << "(backingStoreAttached "_s << backingStoreAttachedForTesting() << ")\n"_s;

    if (m_transform && !m_transform->isIdentity()) {
        ts << indent << "(transform "_s;
        ts << '[' << m_transform->m11() << ' ' << m_transform->m12() << ' ' << m_transform->m13() << ' ' << m_transform->m14() << "] "_s;
        ts << '[' << m_transform->m21() << ' ' << m_transform->m22() << ' ' << m_transform->m23() << ' ' << m_transform->m24() << "] "_s;
        ts << '[' << m_transform->m31() << ' ' << m_transform->m32() << ' ' << m_transform->m33() << ' ' << m_transform->m34() << "] "_s;
        ts << '[' << m_transform->m41() << ' ' << m_transform->m42() << ' ' << m_transform->m43() << ' ' << m_transform->m44() << "])\n"_s;
    }

    // Avoid dumping the sublayer transform on the root layer, because it's used for geometry flipping, whose behavior
    // differs between platforms.
    if (parent() && m_childrenTransform && !m_childrenTransform->isIdentity()) {
        ts << indent << "(childrenTransform "_s;
        ts << '[' << m_childrenTransform->m11() << ' ' << m_childrenTransform->m12() << ' ' << m_childrenTransform->m13() << ' ' << m_childrenTransform->m14() << "] "_s;
        ts << '[' << m_childrenTransform->m21() << ' ' << m_childrenTransform->m22() << ' ' << m_childrenTransform->m23() << ' ' << m_childrenTransform->m24() << "] "_s;
        ts << '[' << m_childrenTransform->m31() << ' ' << m_childrenTransform->m32() << ' ' << m_childrenTransform->m33() << ' ' << m_childrenTransform->m34() << "] "_s;
        ts << '[' << m_childrenTransform->m41() << ' ' << m_childrenTransform->m42() << ' ' << m_childrenTransform->m43() << ' ' << m_childrenTransform->m44() << "])\n"_s;
    }

    if (m_maskLayer) {
        ts << indent << "(mask layer"_s;
        if (options & LayerTreeAsTextOptions::Debug)
            ts << ' ' << m_maskLayer.get();
        ts << ")\n"_s;

        TextStream::IndentScope indentScope(ts);
        m_maskLayer->dumpLayer(ts, options);
    }

    if (m_replicaLayer) {
        ts << indent << "(replica layer"_s;
        if (options & LayerTreeAsTextOptions::Debug)
            ts << ' ' << m_replicaLayer.get();
        ts << ")\n"_s;

        TextStream::IndentScope indentScope(ts);
        m_replicaLayer->dumpLayer(ts, options);
    }

    if (m_replicatedLayer) {
        ts << indent << "(replicated layer"_s;
        if (options & LayerTreeAsTextOptions::Debug)
            ts << ' ' << m_replicatedLayer;
        ts << ")\n"_s;
    }

    if (options & LayerTreeAsTextOptions::IncludeRepaintRects && repaintRectMap().contains(this) && !repaintRectMap().get(this).isEmpty() && client().shouldDumpPropertyForLayer(this, "repaintRects"_s, options)) {
        ts << indent << "(repaint rects\n"_s;
        for (size_t i = 0; i < repaintRectMap().get(this).size(); ++i) {
            if (repaintRectMap().get(this)[i].isEmpty())
                continue;

            TextStream::IndentScope indentScope(ts);
            ts << indent << "(rect "_s;
            ts << repaintRectMap().get(this)[i].x() << ' ';
            ts << repaintRectMap().get(this)[i].y() << ' ';
            ts << repaintRectMap().get(this)[i].width() << ' ';
            ts << repaintRectMap().get(this)[i].height();
            ts << ")\n"_s;
        }
        ts << indent << ")\n"_s;
    }

    if (options & LayerTreeAsTextOptions::IncludeEventRegion && !m_eventRegion.isEmpty()) {
        ts << indent << "(event region"_s << m_eventRegion;
        ts << indent << ")\n"_s;
    }
    
#if ENABLE(SCROLLING_THREAD)
    if ((options & LayerTreeAsTextOptions::Debug) && m_scrollingNodeID)
        ts << indent << "(scrolling node "_s << m_scrollingNodeID << ")\n"_s;
#endif

    if (options & LayerTreeAsTextOptions::IncludePaintingPhases && paintingPhase())
        ts << indent << "(paintingPhases "_s << paintingPhase() << ")\n"_s;

    dumpAdditionalProperties(ts, options);
    
    if (m_children.size()) {
        TextStream childrenStream;
        
        childrenStream.increaseIndent(ts.indent());
        unsigned totalChildCount = 0;
        dumpChildren(childrenStream, m_children, totalChildCount, options);

        if (totalChildCount) {
            ts << indent << "(children "_s << totalChildCount << "\n"_s;
            ts << childrenStream.release();
            ts << indent << ")\n"_s;
        }
    }
}

TextStream& operator<<(TextStream& ts, const Vector<PlatformLayerIdentifier>& layers)
{
    for (size_t i = 0; i < layers.size(); ++i) {
        if (i)
            ts << " ";
        ts << layers[i];
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, GraphicsLayerPaintingPhase phase)
{
    switch (phase) {
    case GraphicsLayerPaintingPhase::Background: ts << "background"; break;
    case GraphicsLayerPaintingPhase::Foreground: ts << "foreground"; break;
    case GraphicsLayerPaintingPhase::Mask: ts << "mask"; break;
    case GraphicsLayerPaintingPhase::ClipPath: ts << "clip-path"; break;
    case GraphicsLayerPaintingPhase::OverflowContents: ts << "overflow-contents"; break;
    case GraphicsLayerPaintingPhase::CompositedScroll: ts << "composited-scroll"; break;
    case GraphicsLayerPaintingPhase::ChildClippingMask: ts << "child-clipping-mask"; break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, const GraphicsLayer::CustomAppearance& customAppearance)
{
    switch (customAppearance) {
    case GraphicsLayer::CustomAppearance::None: ts << "none"; break;
    case GraphicsLayer::CustomAppearance::ScrollingShadow: ts << "scrolling-shadow"; break;
    }
    return ts;
}

String GraphicsLayer::layerTreeAsText(OptionSet<LayerTreeAsTextOptions> options) const
{
    TextStream ts(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);

    dumpLayer(ts, options);
    return ts.release();
}

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
void showGraphicsLayerTree(const WebCore::GraphicsLayer* layer)
{
    if (!layer)
        return;

    String output = layer->layerTreeAsText(WebCore::AllLayerTreeAsTextOptions);
    WTFLogAlways("%s\n", output.utf8().data());

    // The tree is too large to print to the os log so save the tree output
    // to a file in case we don't have easy access to stderr.
    auto [tempFilePath, fileHandle] = FileSystem::openTemporaryFile("GraphicsLayerTree"_s);
    if (FileSystem::isHandleValid(fileHandle)) {
        FileSystem::writeToFile(fileHandle, output.utf8().span());
        FileSystem::closeFile(fileHandle);
        WTFLogAlways("Saved GraphicsLayer Tree to %s", tempFilePath.utf8().data());
    } else
        WTFLogAlways("Failed to open temporary file for saving the GraphicsLayer Tree.");
}
#endif
