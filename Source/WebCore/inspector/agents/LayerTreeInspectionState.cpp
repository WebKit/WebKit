/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LayerTreeInspectionState.h"

#include "ColorSpace.h"
#include "FloatQuad.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "ImageBuffer.h"
#include "ImageUtilities.h"
#include "IntRect.h"
#include "LocalFrameView.h"
#include "PixelFormat.h"
#include "PseudoElement.h"
#include "RenderChildIterator.h"
#include "RenderElementInlines.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(LayerTreeInspectionState);

LayerTreeInspectionState::LayerTreeInspectionState(LayerTreeInspectionScope scope)
    : m_scope(scope)
{
}

LayerTreeInspectionState::~LayerTreeInspectionState() = default;

void LayerTreeInspectionState::reset()
{
    m_documentLayerToIdMap.clear();
    m_idToLayer.clear();
    m_pseudoElementToIdMap.clear();
    m_idToPseudoElement.clear();
}

void LayerTreeInspectionState::renderLayerDestroyed(const RenderLayer& renderLayer)
{
    unbind(&renderLayer);
}

void LayerTreeInspectionState::pseudoElementDestroyed(PseudoElement& pseudoElement)
{
    unbindPseudoElement(&pseudoElement);
}

Ref<JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>> LayerTreeInspectionState::layersForRenderer(RenderElement& renderer, NOESCAPE const NodeIdProvider& nodeIdProvider)
{
    removeDestroyedLayers();

    auto layers = JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>::create();

    gatherLayersUsingRenderObjectHierarchy(renderer, renderer.view().frameView(), layers, nodeIdProvider);

    return layers;
}

static Ref<Inspector::Protocol::LayerTree::IntRect> buildObjectForIntRect(const IntRect& rect)
{
    return Inspector::Protocol::LayerTree::IntRect::create()
        .setX(rect.x())
        .setY(rect.y())
        .setWidth(rect.width())
        .setHeight(rect.height())
        .release();
}

static IntRect rectInScope(const RenderElement& renderer, IntRect rect, LayerTreeInspectionScope scope, const LocalFrameView& rootView)
{
    RefPtr<const LocalFrameView> view = &renderer.view().frameView();
    if (scope == LayerTreeInspectionScope::Frame)
        return view->contentsToView(rect);

    while (view && view != &rootView) {
        RefPtr parentView = dynamicDowncast<LocalFrameView>(view->parent());
        if (!parentView)
            break;
        rect = parentView->viewToContents(view->convertToContainingView(view->contentsToView(rect)));
        view = WTF::move(parentView);
    }
    return rect;
}

static RefPtr<LocalFrameView> includedContentFrameView(const RenderElement& renderer, LayerTreeInspectionScope scope)
{
    if (scope != LayerTreeInspectionScope::Page)
        return nullptr;
    CheckedPtr widgetRenderer = dynamicDowncast<RenderWidget>(renderer);
    return widgetRenderer ? dynamicDowncast<LocalFrameView>(widgetRenderer->widget()) : nullptr;
}

void LayerTreeInspectionState::gatherLayersUsingRenderObjectHierarchy(RenderElement& renderer, const LocalFrameView& rootView, JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>& layers, NOESCAPE const NodeIdProvider& nodeIdProvider)
{
    if (renderer.hasLayer()) {
        gatherLayersUsingRenderLayerHierarchy(CheckedPtr { downcast<RenderLayerModelObject>(renderer).layer() }, rootView, layers, nodeIdProvider);
        return;
    }

    for (CheckedRef child : childrenOfType<RenderElement>(renderer))
        gatherLayersUsingRenderObjectHierarchy(child, rootView, layers, nodeIdProvider);
}

void LayerTreeInspectionState::gatherLayersUsingRenderLayerHierarchy(RenderLayer* renderLayer, const LocalFrameView& rootView, JSON::ArrayOf<Inspector::Protocol::LayerTree::Layer>& layers, NOESCAPE const NodeIdProvider& nodeIdProvider)
{
    CheckedPtr layer = renderLayer;
    if (layer->isComposited())
        layers.addItem(buildObjectForLayer(renderLayer, rootView, nodeIdProvider));

    if (RefPtr contentFrameView = includedContentFrameView(layer->renderer(), m_scope)) {
        if (CheckedPtr contentRenderView = contentFrameView->renderView(); contentRenderView && contentRenderView->hasLayer())
            gatherLayersUsingRenderLayerHierarchy(CheckedPtr { contentRenderView->layer() }.get(), rootView, layers, nodeIdProvider);
    }

    for (layer = layer->firstChild(); layer; layer = layer->nextSibling())
        gatherLayersUsingRenderLayerHierarchy(layer.get(), rootView, layers, nodeIdProvider);
}

Ref<Inspector::Protocol::LayerTree::Layer> LayerTreeInspectionState::buildObjectForLayer(RenderLayer* renderLayer, const LocalFrameView& rootView, NOESCAPE const NodeIdProvider& nodeIdProvider)
{
    RenderElement* renderer = &renderLayer->renderer();
    RenderLayerBacking* backing = renderLayer->backing();
    RefPtr<Node> node = renderer->element();

    bool isReflection = renderLayer->isReflection();
    bool isGenerated = (isReflection ? renderer->parent() : renderer)->isBeforeOrAfterContent();
    bool isAnonymous = renderer->isAnonymous();

    if (renderer->isRenderView())
        node = &renderer->document();
    else if (isReflection && isGenerated)
        node = renderer->parent()->generatingElement();
    else if (isGenerated)
        node = renderer->generatingElement();
    else if (isReflection || isAnonymous)
        node = renderer->parent()->element();

    // Basic set of properties.
    auto layerObject = Inspector::Protocol::LayerTree::Layer::create()
        .setLayerId(bind(renderLayer))
        .setNodeId(nodeIdProvider(node.get()))
        .setBounds(buildObjectForIntRect(rectInScope(*renderer, renderer->absoluteBoundingBoxRect(), m_scope, rootView)))
        .setMemory(backing->backingStoreMemoryEstimate())
        .setCompositedBounds(buildObjectForIntRect(enclosingIntRect(backing->compositedBounds())))
        .setPaintCount(backing->graphicsLayer()->repaintCount())
        .release();

    if (CheckedPtr widgetRenderer = dynamicDowncast<RenderWidget>(*renderer); widgetRenderer && is<FrameView>(widgetRenderer->widget()) && !includedContentFrameView(*widgetRenderer, m_scope)) {
        // FIXME: This is only the bounding box of a transformed owner, so the frame's layers are not scaled or rotated with it.
        auto contentFrameRect = widgetRenderer->absoluteContentQuad().enclosingBoundingBox();
        layerObject->setContentFrameRect(buildObjectForIntRect(rectInScope(*renderer, contentFrameRect, m_scope, rootView)));
    }

    if (node && node->shadowHost())
        layerObject->setIsInShadowTree(true);

    if (isReflection)
        layerObject->setIsReflection(true);

    if (isGenerated) {
        if (isReflection)
            renderer = renderer->parent();
        layerObject->setIsGeneratedContent(true);
        layerObject->setPseudoElementId(bindPseudoElement(RefPtr { downcast<PseudoElement>(renderer->element()) }.get()));
        if (renderer->isBeforeContent())
            layerObject->setPseudoElement("before"_s);
        else if (renderer->isAfterContent())
            layerObject->setPseudoElement("after"_s);
    }

    // FIXME: RenderView is now really anonymous but don't tell about it to the frontend before making sure it can handle it.
    if (isAnonymous && !renderer->isRenderView()) {
        layerObject->setIsAnonymous(true);
        CheckedRef style = renderer->style();
        if (style->pseudoElementType() == PseudoElementType::FirstLetter)
            layerObject->setPseudoElement("first-letter"_s);
        else if (style->pseudoElementType() == PseudoElementType::FirstLine)
            layerObject->setPseudoElement("first-line"_s);
    }

    return layerObject;
}

Inspector::CommandResult<Ref<Inspector::Protocol::LayerTree::CompositingReasons>> LayerTreeInspectionState::reasonsForCompositingLayer(const Inspector::Protocol::LayerTree::LayerId& layerId)
{
    const CheckedPtr renderLayer = m_idToLayer.get(layerId);

    if (!renderLayer)
        return makeUnexpected("Missing render layer for given layerId"_s);

    OptionSet<CompositingReason> reasons = protect(renderLayer->compositor())->reasonsForCompositing(*renderLayer);
    auto compositingReasons = Inspector::Protocol::LayerTree::CompositingReasons::create().release();

    if (reasons.contains(CompositingReason::Transform3D))
        compositingReasons->setTransform3D(true);

    if (reasons.contains(CompositingReason::Video))
        compositingReasons->setVideo(true);
    else if (reasons.contains(CompositingReason::Canvas))
        compositingReasons->setCanvas(true);
    else if (reasons.contains(CompositingReason::Plugin))
        compositingReasons->setPlugin(true);
    else if (reasons.contains(CompositingReason::IFrame))
        compositingReasons->setIFrame(true);
    else if (reasons.contains(CompositingReason::Model))
        compositingReasons->setModel(true);

    if (reasons.contains(CompositingReason::BackfaceVisibilityHidden))
        compositingReasons->setBackfaceVisibilityHidden(true);

    if (reasons.contains(CompositingReason::ClipsCompositingDescendants))
        compositingReasons->setClipsCompositingDescendants(true);

    if (reasons.contains(CompositingReason::Animation))
        compositingReasons->setAnimation(true);

    if (reasons.contains(CompositingReason::Filters))
        compositingReasons->setFilters(true);

    if (reasons.contains(CompositingReason::PositionFixed))
        compositingReasons->setPositionFixed(true);

    if (reasons.contains(CompositingReason::PositionSticky))
        compositingReasons->setPositionSticky(true);

    if (reasons.contains(CompositingReason::OverflowScrolling))
        compositingReasons->setOverflowScrollingTouch(true);

    // FIXME: handle OverflowScrollPositioning (webkit.org/b/195985).

    if (reasons.contains(CompositingReason::Stacking))
        compositingReasons->setStacking(true);

    if (reasons.contains(CompositingReason::Overlap))
        compositingReasons->setOverlap(true);

    if (reasons.contains(CompositingReason::NegativeZIndexChildren))
        compositingReasons->setNegativeZIndexChildren(true);

    if (reasons.contains(CompositingReason::TransformWithCompositedDescendants))
        compositingReasons->setTransformWithCompositedDescendants(true);

    if (reasons.contains(CompositingReason::OpacityWithCompositedDescendants))
        compositingReasons->setOpacityWithCompositedDescendants(true);

    if (reasons.contains(CompositingReason::MaskWithCompositedDescendants))
        compositingReasons->setMaskWithCompositedDescendants(true);

    if (reasons.contains(CompositingReason::ReflectionWithCompositedDescendants))
        compositingReasons->setReflectionWithCompositedDescendants(true);

    if (reasons.contains(CompositingReason::FilterWithCompositedDescendants))
        compositingReasons->setFilterWithCompositedDescendants(true);

    if (reasons.contains(CompositingReason::BlendingWithCompositedDescendants))
        compositingReasons->setBlendingWithCompositedDescendants(true);

    if (reasons.contains(CompositingReason::IsolatesCompositedBlendingDescendants))
        compositingReasons->setIsolatesCompositedBlendingDescendants(true);

    if (reasons.contains(CompositingReason::Perspective))
        compositingReasons->setPerspective(true);

    if (reasons.contains(CompositingReason::Preserve3D))
        compositingReasons->setPreserve3D(true);

    if (reasons.contains(CompositingReason::WillChange))
        compositingReasons->setWillChange(true);

    if (reasons.contains(CompositingReason::Root))
        compositingReasons->setRoot(true);

    if (reasons.contains(CompositingReason::BackdropRoot))
        compositingReasons->setBackdropRoot(true);

    return compositingReasons;
}

Inspector::CommandResult<String> LayerTreeInspectionState::requestContent(const Inspector::Protocol::LayerTree::LayerId& layerId)
{
    CheckedPtr renderLayer = m_idToLayer.get(layerId);
    if (!renderLayer)
        return makeUnexpected("Missing render layer for given layerId"_s);

    auto* backing = renderLayer->backing();
    if (!backing)
        return makeUnexpected("Layer is not composited"_s);

    RefPtr graphicsLayer = backing->graphicsLayer();
    if (!graphicsLayer)
        return makeUnexpected("Missing graphics layer"_s);

    FloatSize layerSize = graphicsLayer->size();
    if (layerSize.isEmpty())
        return emptyString();

    // Limit scale factor for large layers to prevent excessive memory usage.
    constexpr float maxScaleFactor = 2;
    constexpr float maxSnapshotDimension = 4096;
    float scaleFactor = std::min({
        maxSnapshotDimension / layerSize.width(),
        maxSnapshotDimension / layerSize.height(),
        maxScaleFactor,
    });

    auto imageBuffer = ImageBuffer::create(layerSize, RenderingMode::Unaccelerated, RenderingPurpose::Snapshot, scaleFactor, ColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!imageBuffer)
        return makeUnexpected("Failed to create image buffer"_s);

    graphicsLayer->paintGraphicsLayerContents(imageBuffer->context(), { { }, layerSize });

    return encodeDataURL(WTF::move(imageBuffer), "image/png"_s);
}

String LayerTreeInspectionState::bind(const RenderLayer* layer)
{
    if (!layer)
        return emptyString();
    return m_documentLayerToIdMap.ensure(layer, [this, layer] {
        auto identifier = IdentifiersFactory::createIdentifier();
        m_idToLayer.set(identifier, InlineWeakPtr { layer });
        return identifier;
    }).iterator->value;
}

void LayerTreeInspectionState::removeDestroyedLayers()
{
    // RenderLayer::clearBacking doesn't report layers destroyed with the render tree.
    m_documentLayerToIdMap.removeIf([&](auto& entry) {
        if (m_idToLayer.get(entry.value))
            return false;
        m_idToLayer.remove(entry.value);
        return true;
    });
}

void LayerTreeInspectionState::unbind(const RenderLayer* layer)
{
    auto identifier = m_documentLayerToIdMap.take(layer);
    if (identifier.isNull())
        return;
    m_idToLayer.remove(identifier);
}

String LayerTreeInspectionState::bindPseudoElement(PseudoElement* pseudoElement)
{
    if (!pseudoElement)
        return emptyString();
    return m_pseudoElementToIdMap.ensure(*pseudoElement, [this, pseudoElement] {
        auto identifier = IdentifiersFactory::createIdentifier();
        m_idToPseudoElement.set(identifier, pseudoElement);
        return identifier;
    }).iterator->value;
}

void LayerTreeInspectionState::unbindPseudoElement(PseudoElement* pseudoElement)
{
    if (!pseudoElement)
        return;
    auto identifier = m_pseudoElementToIdMap.take(*pseudoElement);
    if (identifier.isNull())
        return;
    m_idToPseudoElement.remove(identifier);
}

} // namespace WebCore
