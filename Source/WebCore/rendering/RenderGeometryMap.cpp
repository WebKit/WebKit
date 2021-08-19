/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "RenderGeometryMap.h"

#include "RenderFragmentedFlow.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include "TransformState.h"
#include <wtf/SetForScope.h>

namespace WebCore {

RenderGeometryMap::RenderGeometryMap(OptionSet<MapCoordinatesMode> flags)
    : m_mapCoordinatesFlags(flags)
{
}

RenderGeometryMap::~RenderGeometryMap() = default;

void RenderGeometryMap::mapToContainer(TransformState& transformState, const RenderLayerModelObject* container) const
{
    // If the mapping includes something like columns, we have to go via renderers.
    if (hasNonUniformStep()) {
        m_mapping.last().m_renderer->mapLocalToContainer(container, transformState, ApplyContainerFlip | m_mapCoordinatesFlags);
        transformState.flatten();
        return;
    }
    
    bool inFixed = false;
#if ASSERT_ENABLED
    bool foundContainer = !container || (m_mapping.size() && m_mapping[0].m_renderer == container);
#endif

    for (int i = m_mapping.size() - 1; i >= 0; --i) {
        const RenderGeometryMapStep& currentStep = m_mapping[i];

        // If container is the RenderView (step 0) we want to apply its scroll offset.
        if (i > 0 && currentStep.m_renderer == container) {
#if ASSERT_ENABLED
            foundContainer = true;
#endif
            break;
        }

        // If this box has a transform, it acts as a fixed position container
        // for fixed descendants, which prevents the propagation of 'fixed'
        // unless the layer itself is also fixed position.
        if (i && currentStep.m_hasTransform && !currentStep.m_isFixedPosition)
            inFixed = false;
        else if (currentStep.m_isFixedPosition)
            inFixed = true;

        if (!i) {
            // The root gets special treatment for fixed position
            if (inFixed)
                transformState.move(currentStep.m_offset.width(), currentStep.m_offset.height());

            // A null container indicates mapping through the RenderView, so including its transform (the page scale).
            if (!container && currentStep.m_transform)
                transformState.applyTransform(*currentStep.m_transform.get());
        } else {
            TransformState::TransformAccumulation accumulate = currentStep.m_accumulatingTransform ? TransformState::AccumulateTransform : TransformState::FlattenTransform;
            if (currentStep.m_transform)
                transformState.applyTransform(*currentStep.m_transform.get(), accumulate);
            else
                transformState.move(currentStep.m_offset.width(), currentStep.m_offset.height(), accumulate);
        }
    }

    ASSERT(foundContainer);
    transformState.flatten();    
}

FloatPoint RenderGeometryMap::mapToContainer(const FloatPoint& p, const RenderLayerModelObject* container) const
{
    FloatPoint result;
#if ASSERT_ENABLED
    FloatPoint rendererMappedResult = m_mapping.last().m_renderer->localToAbsolute(p, m_mapCoordinatesFlags);
#endif

    if (!hasFixedPositionStep() && !hasTransformStep() && !hasNonUniformStep() && (!container || (m_mapping.size() && container == m_mapping[0].m_renderer))) {
        result = p;
        result.move(m_accumulatedOffset);
        ASSERT(m_accumulatedOffsetMightBeSaturated || areEssentiallyEqual(rendererMappedResult, result));
    } else {
        TransformState transformState(TransformState::ApplyTransformDirection, p);
        mapToContainer(transformState, container);
        result = transformState.lastPlanarPoint();
        ASSERT(areEssentiallyEqual(rendererMappedResult, result));
    }

    return result;
}

FloatQuad RenderGeometryMap::mapToContainer(const FloatRect& rect, const RenderLayerModelObject* container) const
{
    FloatQuad result;
    
    if (!hasFixedPositionStep() && !hasTransformStep() && !hasNonUniformStep() && (!container || (m_mapping.size() && container == m_mapping[0].m_renderer))) {
        result = rect;
        result.move(m_accumulatedOffset);
    } else {
        TransformState transformState(TransformState::ApplyTransformDirection, rect.center(), rect);
        mapToContainer(transformState, container);
        result = transformState.lastPlanarQuad();
    }

    return result;
}

void RenderGeometryMap::pushMappingsToAncestor(const RenderObject* renderer, const RenderLayerModelObject* ancestorRenderer)
{
    // We need to push mappings in reverse order here, so do insertions rather than appends.
    SetForScope<size_t> positionChange(m_insertionPosition, m_mapping.size());
    do {
        renderer = renderer->pushMappingToContainer(ancestorRenderer, *this);
    } while (renderer && renderer != ancestorRenderer);

    ASSERT(m_mapping.isEmpty() || m_mapping[0].m_renderer->isRenderView());
}

static bool canMapBetweenRenderersViaLayers(const RenderLayerModelObject& renderer, const RenderLayerModelObject& ancestor)
{
    for (const RenderElement* current = &renderer; ; current = current->parent()) {
        const RenderStyle& style = current->style();
        if (current->isFixedPositioned() || style.isFlippedBlocksWritingMode())
            return false;

        if (current->hasTransformRelatedProperty() && !current->style().preserves3D())
            return false;
        
        if (current->isRenderFragmentedFlow())
            return false;

        if (current->isSVGRoot())
            return false;

        if (current == &ancestor)
            break;
    }

    return true;
}

void RenderGeometryMap::pushMappingsToAncestor(const RenderLayer* layer, const RenderLayer* ancestorLayer, bool respectTransforms)
{
    OptionSet<MapCoordinatesMode> newFlags = m_mapCoordinatesFlags;
    if (!respectTransforms)
        newFlags.remove(UseTransforms);

    SetForScope<OptionSet<MapCoordinatesMode>> flagsChange(m_mapCoordinatesFlags, newFlags);

    const RenderLayerModelObject& renderer = layer->renderer();

    // We have to visit all the renderers to detect flipped blocks. This might defeat the gains
    // from mapping via layers.
    bool canConvertInLayerTree = ancestorLayer ? canMapBetweenRenderersViaLayers(layer->renderer(), ancestorLayer->renderer()) : false;

    if (canConvertInLayerTree) {
        LayoutSize layerOffset = layer->offsetFromAncestor(ancestorLayer);
        
        // The RenderView must be pushed first.
        if (!m_mapping.size()) {
            ASSERT(ancestorLayer->renderer().isRenderView());
            pushMappingsToAncestor(&ancestorLayer->renderer(), nullptr);
        }

        SetForScope<size_t> positionChange(m_insertionPosition, m_mapping.size());
        push(&renderer, layerOffset, /*accumulatingTransform*/ true, /*isNonUniform*/ false, /*isFixedPosition*/ false, /*hasTransform*/ false);
        return;
    }
    const RenderLayerModelObject* ancestorRenderer = ancestorLayer ? &ancestorLayer->renderer() : nullptr;
    pushMappingsToAncestor(&renderer, ancestorRenderer);
}

void RenderGeometryMap::push(const RenderObject* renderer, const LayoutSize& offsetFromContainer, bool accumulatingTransform, bool isNonUniform, bool isFixedPosition, bool hasTransform)
{
    ASSERT(m_insertionPosition != notFound);

    m_mapping.insert(m_insertionPosition, RenderGeometryMapStep(renderer, accumulatingTransform, isNonUniform, isFixedPosition, hasTransform));

    RenderGeometryMapStep& step = m_mapping[m_insertionPosition];
    step.m_offset = offsetFromContainer;

    stepInserted(step);
}

void RenderGeometryMap::push(const RenderObject* renderer, const TransformationMatrix& t, bool accumulatingTransform, bool isNonUniform, bool isFixedPosition, bool hasTransform)
{
    ASSERT(m_insertionPosition != notFound);

    m_mapping.insert(m_insertionPosition, RenderGeometryMapStep(renderer, accumulatingTransform, isNonUniform, isFixedPosition, hasTransform));
    
    RenderGeometryMapStep& step = m_mapping[m_insertionPosition];
    if (!t.isIntegerTranslation())
        step.m_transform = makeUnique<TransformationMatrix>(t);
    else
        step.m_offset = LayoutSize(t.e(), t.f());

    stepInserted(step);
}

void RenderGeometryMap::pushView(const RenderView* view, const LayoutSize& scrollOffset, const TransformationMatrix* t)
{
    ASSERT(m_insertionPosition != notFound);
    ASSERT(!m_insertionPosition); // The view should always be the first step.

    m_mapping.insert(m_insertionPosition, RenderGeometryMapStep(view, false, false, false, t));
    
    RenderGeometryMapStep& step = m_mapping[m_insertionPosition];
    step.m_offset = scrollOffset;
    if (t)
        step.m_transform = makeUnique<TransformationMatrix>(*t);
    
    stepInserted(step);
}

void RenderGeometryMap::pushRenderFragmentedFlow(const RenderFragmentedFlow* fragmentedFlow)
{
    m_mapping.append(RenderGeometryMapStep(fragmentedFlow, false, false, false, false));
    stepInserted(m_mapping.last());
}

void RenderGeometryMap::popMappingsToAncestor(const RenderLayerModelObject* ancestorRenderer)
{
    ASSERT(m_mapping.size());

    while (m_mapping.size() && m_mapping.last().m_renderer != ancestorRenderer) {
        stepRemoved(m_mapping.last());
        m_mapping.removeLast();
    }
}

void RenderGeometryMap::popMappingsToAncestor(const RenderLayer* ancestorLayer)
{
    const RenderLayerModelObject* ancestorRenderer = ancestorLayer ? &ancestorLayer->renderer() : 0;
    popMappingsToAncestor(ancestorRenderer);
}

void RenderGeometryMap::stepInserted(const RenderGeometryMapStep& step)
{
    // RenderView's offset, is only applied when we have fixed-positions.
    if (!step.m_renderer->isRenderView()) {
        m_accumulatedOffset += step.m_offset;
#if ASSERT_ENABLED
        m_accumulatedOffsetMightBeSaturated |= m_accumulatedOffset.mightBeSaturated();
#endif
    }

    if (step.m_isNonUniform)
        ++m_nonUniformStepsCount;

    if (step.m_transform)
        ++m_transformedStepsCount;
    
    if (step.m_isFixedPosition)
        ++m_fixedStepsCount;
}

void RenderGeometryMap::stepRemoved(const RenderGeometryMapStep& step)
{
    // RenderView's offset, is only applied when we have fixed-positions.
    if (!step.m_renderer->isRenderView()) {
        m_accumulatedOffset -= step.m_offset;
#if ASSERT_ENABLED
        m_accumulatedOffsetMightBeSaturated |= m_accumulatedOffset.mightBeSaturated();
#endif
    }

    if (step.m_isNonUniform) {
        ASSERT(m_nonUniformStepsCount);
        --m_nonUniformStepsCount;
    }

    if (step.m_transform) {
        ASSERT(m_transformedStepsCount);
        --m_transformedStepsCount;
    }

    if (step.m_isFixedPosition) {
        ASSERT(m_fixedStepsCount);
        --m_fixedStepsCount;
    }
}

} // namespace WebCore
