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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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
#include "RenderGeometryMap.h"

#include "RenderLayer.h"
#include "RenderView.h"
#include "TransformState.h"
#include <wtf/TemporaryChange.h>

namespace WebCore {

RenderGeometryMap::RenderGeometryMap()
    : m_insertionPosition(notFound)
    , m_nonUniformStepsCount(0)
    , m_transformedStepsCount(0)
    , m_fixedStepsCount(0)
{
}

RenderGeometryMap::~RenderGeometryMap()
{
}

FloatPoint RenderGeometryMap::absolutePoint(const FloatPoint& p) const
{
    FloatPoint result;
    
    if (!hasFixedPositionStep() && !hasTransformStep() && !hasNonUniformStep())
        result = p + m_accumulatedOffset;
    else {
        TransformState transformState(TransformState::ApplyTransformDirection, p);
        mapToAbsolute(transformState);
        result = transformState.lastPlanarPoint();
    }

#if !ASSERT_DISABLED
    FloatPoint rendererMappedResult = m_mapping.last().m_renderer->localToAbsolute(p, false, true);
    ASSERT(rendererMappedResult == result);
#endif

    return result;
}

FloatRect RenderGeometryMap::absoluteRect(const FloatRect& rect) const
{
    FloatRect result;
    
    if (!hasFixedPositionStep() && !hasTransformStep() && !hasNonUniformStep()) {
        result = rect;
        result.move(m_accumulatedOffset);
    } else {
        TransformState transformState(TransformState::ApplyTransformDirection, rect.center(), rect);
        mapToAbsolute(transformState);
        result = transformState.lastPlanarQuad().boundingBox();
    }

#if !ASSERT_DISABLED
    FloatRect rendererMappedResult = m_mapping.last().m_renderer->localToAbsoluteQuad(rect).boundingBox();
    // Inspector creates renderers with negative width <https://bugs.webkit.org/show_bug.cgi?id=87194>.
    // Taking FloatQuad bounds avoids spurious assertions because of that.
    ASSERT(enclosingIntRect(rendererMappedResult) == enclosingIntRect(FloatQuad(result).boundingBox()));
#endif

    return result;
}

void RenderGeometryMap::mapToAbsolute(TransformState& transformState) const
{
    // If the mapping includes something like columns, we have to go via renderers.
    if (hasNonUniformStep()) {
        m_mapping.last().m_renderer->mapLocalToContainer(0, transformState, UseTransforms | ApplyContainerFlip | SnapOffsetForTransforms);
        return;
    }
    
    bool inFixed = false;

    for (int i = m_mapping.size() - 1; i >= 0; --i) {
        const RenderGeometryMapStep& currentStep = m_mapping[i];

        // If this box has a transform, it acts as a fixed position container
        // for fixed descendants, which prevents the propagation of 'fixed'
        // unless the layer itself is also fixed position.
        if (currentStep.m_hasTransform && !currentStep.m_isFixedPosition)
            inFixed = false;
        else if (currentStep.m_isFixedPosition)
            inFixed = true;

        if (!i) {
            if (currentStep.m_transform)
                transformState.applyTransform(*currentStep.m_transform.get());

            // The root gets special treatment for fixed position
            if (inFixed)
                transformState.move(currentStep.m_offset.width(), currentStep.m_offset.height());
        } else {
            TransformState::TransformAccumulation accumulate = currentStep.m_accumulatingTransform ? TransformState::AccumulateTransform : TransformState::FlattenTransform;
            if (currentStep.m_transform)
                transformState.applyTransform(*currentStep.m_transform.get(), accumulate);
            else
                transformState.move(currentStep.m_offset.width(), currentStep.m_offset.height(), accumulate);
        }
    }

    transformState.flatten();    
}

void RenderGeometryMap::pushMappingsToAncestor(const RenderObject* renderer, const RenderBoxModelObject* ancestorRenderer)
{
    // We need to push mappings in reverse order here, so do insertions rather than appends.
    TemporaryChange<size_t> positionChange(m_insertionPosition, m_mapping.size());
    do {
        renderer = renderer->pushMappingToContainer(ancestorRenderer, *this);
    } while (renderer && renderer != ancestorRenderer);

    ASSERT(m_mapping.isEmpty() || m_mapping[0].m_renderer->isRenderView());
}

static bool canMapViaLayer(const RenderLayer* layer)
{
    RenderStyle* style = layer->renderer()->style();
    if (style->position() == FixedPosition || style->isFlippedBlocksWritingMode())
        return false;
    
    if (layer->renderer()->hasColumns() || layer->renderer()->hasTransform())
        return false;

#if ENABLE(SVG)
    if (layer->renderer()->isSVGRoot())
        return false;
#endif

    return true;
}

void RenderGeometryMap::pushMappingsToAncestor(const RenderLayer* layer, const RenderLayer* ancestorLayer)
{
    const RenderObject* renderer = layer->renderer();

    // The simple case can be handled fast in the layer tree.
    bool canConvertInLayerTree = ancestorLayer ? canMapViaLayer(ancestorLayer) : false;
    for (const RenderLayer* current = layer; current != ancestorLayer && canConvertInLayerTree; current = current->parent())
        canConvertInLayerTree = canMapViaLayer(current);

    if (canConvertInLayerTree) {
        TemporaryChange<size_t> positionChange(m_insertionPosition, m_mapping.size());
        LayoutPoint layerOffset;
        layer->convertToLayerCoords(ancestorLayer, layerOffset);
        push(renderer, toLayoutSize(layerOffset), /*accumulatingTransform*/ true, /*isNonUniform*/ false, /*isFixedPosition*/ false, /*hasTransform*/ false);
        return;
    }
    const RenderBoxModelObject* ancestorRenderer = ancestorLayer ? ancestorLayer->renderer() : 0;
    pushMappingsToAncestor(renderer, ancestorRenderer);
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
        step.m_transform = adoptPtr(new TransformationMatrix(t));
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
        step.m_transform = adoptPtr(new TransformationMatrix(*t));
    
    stepInserted(step);
}

void RenderGeometryMap::popMappingsToAncestor(const RenderBoxModelObject* ancestorRenderer)
{
    ASSERT(m_mapping.size());

    while (m_mapping.size() && m_mapping.last().m_renderer != ancestorRenderer) {
        stepRemoved(m_mapping.last());
        m_mapping.removeLast();
    }
}

void RenderGeometryMap::popMappingsToAncestor(const RenderLayer* ancestorLayer)
{
    const RenderBoxModelObject* ancestorRenderer = ancestorLayer ? ancestorLayer->renderer() : 0;
    popMappingsToAncestor(ancestorRenderer);
}

void RenderGeometryMap::stepInserted(const RenderGeometryMapStep& step)
{
    // RenderView's offset, is only applied when we have fixed-positions.
    if (!step.m_renderer->isRenderView())
        m_accumulatedOffset += step.m_offset;

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
    if (!step.m_renderer->isRenderView())
        m_accumulatedOffset -= step.m_offset;

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
